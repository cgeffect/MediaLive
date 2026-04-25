#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

namespace {
std::atomic<bool> g_running{true};

void HandleSignal(int) { g_running.store(false); }

std::string AvErr2Str(int err) {
  char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(err, buf, sizeof(buf));
  return std::string(buf);
}

struct TaskConfig {
  std::string stream_id = "stream1";
  std::string host = "127.0.0.1";
  int port = 1935;
  std::string app = "live";
  std::string input_file;
  int width = 1280;
  int height = 720;
  int fps = 30;
  int audio_freq = 1000;
};

std::string BuildTargetUrl(const TaskConfig& cfg) {
  return "rtmp://" + cfg.host + ":" + std::to_string(cfg.port) + "/" + cfg.app + "/" + cfg.stream_id;
}

void ApplyFrameRenderEffect(AVFrame* frame, int64_t frame_index) {
  if (!frame || frame->format != AV_PIX_FMT_YUV420P) {
    return;
  }

  const int width = frame->width;
  const int height = frame->height;
  const int line = frame->linesize[0];

  // Simple frame-level render effect: moving bright vertical band.
  int band_start = static_cast<int>((frame_index * 4) % (width + 80)) - 80;
  int band_end = band_start + 80;

  for (int y = 0; y < height; ++y) {
    uint8_t* row = frame->data[0] + y * line;
    for (int x = 0; x < width; ++x) {
      if (x >= band_start && x < band_end) {
        int v = row[x] + 28;
        row[x] = static_cast<uint8_t>(v > 255 ? 255 : v);
      }
    }
  }
}

struct ApiPipeline {
  AVFormatContext* in_ctx = nullptr;
  AVFormatContext* out_ctx = nullptr;

  int video_in_idx = -1;
  int audio_in_idx = -1;
  int video_out_idx = -1;
  int audio_out_idx = -1;

  AVCodecContext* vdec_ctx = nullptr;
  AVCodecContext* venc_ctx = nullptr;

  AVFrame* dec_frame = nullptr;
  AVFrame* enc_frame = nullptr;
  AVPacket* in_pkt = nullptr;
  AVPacket* out_pkt = nullptr;

  SwsContext* sws_ctx = nullptr;

  int64_t frame_index = 0;
  int64_t wall_start_us = 0;
  int64_t media_start_us = AV_NOPTS_VALUE;

  ~ApiPipeline() {
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (dec_frame) av_frame_free(&dec_frame);
    if (enc_frame) av_frame_free(&enc_frame);
    if (in_pkt) av_packet_free(&in_pkt);
    if (out_pkt) av_packet_free(&out_pkt);
    if (vdec_ctx) avcodec_free_context(&vdec_ctx);
    if (venc_ctx) avcodec_free_context(&venc_ctx);
    if (out_ctx) {
      if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&out_ctx->pb);
      }
      avformat_free_context(out_ctx);
    }
    if (in_ctx) avformat_close_input(&in_ctx);
  }
};

int InitInput(ApiPipeline& p, const TaskConfig& cfg) {
  int ret = avformat_open_input(&p.in_ctx, cfg.input_file.c_str(), nullptr, nullptr);
  if (ret < 0) return ret;

  ret = avformat_find_stream_info(p.in_ctx, nullptr);
  if (ret < 0) return ret;

  p.video_in_idx = av_find_best_stream(p.in_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  if (p.video_in_idx < 0) {
    return p.video_in_idx;
  }
  p.audio_in_idx = av_find_best_stream(p.in_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

  AVStream* vin = p.in_ctx->streams[p.video_in_idx];
  const AVCodec* vdec = avcodec_find_decoder(vin->codecpar->codec_id);
  if (!vdec) return AVERROR_DECODER_NOT_FOUND;

  p.vdec_ctx = avcodec_alloc_context3(vdec);
  if (!p.vdec_ctx) return AVERROR(ENOMEM);

  ret = avcodec_parameters_to_context(p.vdec_ctx, vin->codecpar);
  if (ret < 0) return ret;

  ret = avcodec_open2(p.vdec_ctx, vdec, nullptr);
  if (ret < 0) return ret;

  return 0;
}

int InitOutput(ApiPipeline& p, const TaskConfig& cfg) {
  const std::string target_url = BuildTargetUrl(cfg);
  int ret = avformat_alloc_output_context2(&p.out_ctx, nullptr, "flv", target_url.c_str());
  if (ret < 0 || !p.out_ctx) return ret < 0 ? ret : AVERROR_UNKNOWN;

  AVStream* vin = p.in_ctx->streams[p.video_in_idx];
  AVStream* vout = avformat_new_stream(p.out_ctx, nullptr);
  if (!vout) return AVERROR(ENOMEM);
  p.video_out_idx = vout->index;

  const AVCodec* venc = avcodec_find_encoder_by_name("libx264");
  if (!venc) venc = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!venc) return AVERROR_ENCODER_NOT_FOUND;

  p.venc_ctx = avcodec_alloc_context3(venc);
  if (!p.venc_ctx) return AVERROR(ENOMEM);

  p.venc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  p.venc_ctx->codec_id = venc->id;
  p.venc_ctx->width = p.vdec_ctx->width;
  p.venc_ctx->height = p.vdec_ctx->height;
  p.venc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  p.venc_ctx->time_base = av_inv_q(vin->r_frame_rate.num > 0 ? vin->r_frame_rate : AVRational{cfg.fps, 1});
  if (p.venc_ctx->time_base.num <= 0 || p.venc_ctx->time_base.den <= 0) {
    p.venc_ctx->time_base = AVRational{1, cfg.fps};
  }
  p.venc_ctx->framerate = av_inv_q(p.venc_ctx->time_base);
  p.venc_ctx->gop_size = cfg.fps * 2;
  p.venc_ctx->max_b_frames = 0;
  p.venc_ctx->bit_rate = 2000 * 1000;

  av_opt_set(p.venc_ctx->priv_data, "preset", "veryfast", 0);
  av_opt_set(p.venc_ctx->priv_data, "tune", "zerolatency", 0);

  if (p.out_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
    p.venc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  ret = avcodec_open2(p.venc_ctx, venc, nullptr);
  if (ret < 0) return ret;

  ret = avcodec_parameters_from_context(vout->codecpar, p.venc_ctx);
  if (ret < 0) return ret;
  vout->time_base = p.venc_ctx->time_base;

  if (p.audio_in_idx >= 0) {
    AVStream* ain = p.in_ctx->streams[p.audio_in_idx];
    if (ain->codecpar->codec_id == AV_CODEC_ID_AAC) {
      AVStream* aout = avformat_new_stream(p.out_ctx, nullptr);
      if (!aout) return AVERROR(ENOMEM);
      p.audio_out_idx = aout->index;
      ret = avcodec_parameters_copy(aout->codecpar, ain->codecpar);
      if (ret < 0) return ret;
      aout->codecpar->codec_tag = 0;
      aout->time_base = ain->time_base;
    }
  }

  if (!(p.out_ctx->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&p.out_ctx->pb, target_url.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) return ret;
  }

  ret = avformat_write_header(p.out_ctx, nullptr);
  if (ret < 0) return ret;

  p.dec_frame = av_frame_alloc();
  p.enc_frame = av_frame_alloc();
  p.in_pkt = av_packet_alloc();
  p.out_pkt = av_packet_alloc();
  if (!p.dec_frame || !p.enc_frame || !p.in_pkt || !p.out_pkt) return AVERROR(ENOMEM);

  p.enc_frame->format = p.venc_ctx->pix_fmt;
  p.enc_frame->width = p.venc_ctx->width;
  p.enc_frame->height = p.venc_ctx->height;
  ret = av_frame_get_buffer(p.enc_frame, 32);
  if (ret < 0) return ret;

  p.wall_start_us = av_gettime_relative();
  p.media_start_us = AV_NOPTS_VALUE;

  return 0;
}

void PaceByTimestamp(ApiPipeline& p, int64_t dts, AVRational tb) {
  if (dts == AV_NOPTS_VALUE) return;
  int64_t dts_us = av_rescale_q(dts, tb, AVRational{1, AV_TIME_BASE});
  if (p.media_start_us == AV_NOPTS_VALUE) {
    p.media_start_us = dts_us;
  }
  int64_t elapsed_us = av_gettime_relative() - p.wall_start_us;
  int64_t target_us = dts_us - p.media_start_us;
  if (target_us > elapsed_us) {
    av_usleep(static_cast<unsigned int>(target_us - elapsed_us));
  }
}

int ConvertOrCopyToEncoderFrame(ApiPipeline& p, AVFrame* src, AVFrame*& out) {
  if (src->format == p.venc_ctx->pix_fmt && src->width == p.venc_ctx->width && src->height == p.venc_ctx->height) {
    out = src;
    return 0;
  }

  int ret = av_frame_make_writable(p.enc_frame);
  if (ret < 0) return ret;

  if (!p.sws_ctx) {
    p.sws_ctx = sws_getContext(src->width, src->height, static_cast<AVPixelFormat>(src->format),
                               p.venc_ctx->width, p.venc_ctx->height, p.venc_ctx->pix_fmt,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!p.sws_ctx) return AVERROR(EINVAL);
  }

  sws_scale(p.sws_ctx, src->data, src->linesize, 0, src->height, p.enc_frame->data, p.enc_frame->linesize);
  p.enc_frame->pts = src->pts;
  out = p.enc_frame;
  return 0;
}

int WriteVideoPacket(ApiPipeline& p) {
  AVStream* vout = p.out_ctx->streams[p.video_out_idx];
  p.out_pkt->stream_index = p.video_out_idx;

  av_packet_rescale_ts(p.out_pkt, p.venc_ctx->time_base, vout->time_base);
  PaceByTimestamp(p, p.out_pkt->dts, vout->time_base);
  int ret = av_interleaved_write_frame(p.out_ctx, p.out_pkt);
  av_packet_unref(p.out_pkt);
  return ret;
}

int ProcessVideoDecode(ApiPipeline& p, const std::atomic<bool>& running) {
  int ret = avcodec_send_packet(p.vdec_ctx, p.in_pkt);
  if (ret < 0) return ret;

  while (running.load()) {
    ret = avcodec_receive_frame(p.vdec_ctx, p.dec_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
    if (ret < 0) return ret;

    int64_t src_pts = (p.dec_frame->best_effort_timestamp != AV_NOPTS_VALUE)
                          ? p.dec_frame->best_effort_timestamp
                          : p.dec_frame->pts;
    if (src_pts == AV_NOPTS_VALUE) src_pts = p.frame_index;

    p.dec_frame->pts = av_rescale_q(src_pts,
                                    p.in_ctx->streams[p.video_in_idx]->time_base,
                                    p.venc_ctx->time_base);

    AVFrame* render_frame = nullptr;
    ret = ConvertOrCopyToEncoderFrame(p, p.dec_frame, render_frame);
    if (ret < 0) {
      av_frame_unref(p.dec_frame);
      return ret;
    }

    ApplyFrameRenderEffect(render_frame, p.frame_index++);

    ret = avcodec_send_frame(p.venc_ctx, render_frame);
    av_frame_unref(p.dec_frame);
    if (ret < 0) return ret;

    while (running.load()) {
      ret = avcodec_receive_packet(p.venc_ctx, p.out_pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
      if (ret < 0) return ret;

      ret = WriteVideoPacket(p);
      if (ret < 0) return ret;
    }
  }

  return 0;
}

int FlushVideo(ApiPipeline& p, const std::atomic<bool>& running) {
  int ret = avcodec_send_frame(p.venc_ctx, nullptr);
  if (ret < 0) return ret;

  while (running.load()) {
    ret = avcodec_receive_packet(p.venc_ctx, p.out_pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
    if (ret < 0) return ret;
    ret = WriteVideoPacket(p);
    if (ret < 0) return ret;
  }

  return 0;
}

int ProcessAudioCopy(ApiPipeline& p) {
  if (p.audio_out_idx < 0) return 0;

  AVStream* ain = p.in_ctx->streams[p.audio_in_idx];
  AVStream* aout = p.out_ctx->streams[p.audio_out_idx];

  p.in_pkt->stream_index = p.audio_out_idx;
  av_packet_rescale_ts(p.in_pkt, ain->time_base, aout->time_base);
  PaceByTimestamp(p, p.in_pkt->dts, aout->time_base);

  int ret = av_interleaved_write_frame(p.out_ctx, p.in_pkt);
  av_packet_unref(p.in_pkt);
  return ret;
}

int PushByFrameRenderOnce(const TaskConfig& cfg, const std::atomic<bool>& running) {
  ApiPipeline p;

  int ret = InitInput(p, cfg);
  if (ret < 0) {
    std::cerr << "[encoder-pusher] init input failed: " << AvErr2Str(ret) << std::endl;
    return ret;
  }

  ret = InitOutput(p, cfg);
  if (ret < 0) {
    std::cerr << "[encoder-pusher] init output failed: " << AvErr2Str(ret) << std::endl;
    return ret;
  }

  while (running.load()) {
    ret = av_read_frame(p.in_ctx, p.in_pkt);
    if (ret < 0) break;

    if (p.in_pkt->stream_index == p.video_in_idx) {
      ret = ProcessVideoDecode(p, running);
      av_packet_unref(p.in_pkt);
      if (ret < 0) {
        std::cerr << "[encoder-pusher] process video failed: " << AvErr2Str(ret) << std::endl;
        return ret;
      }
      continue;
    }

    if (p.in_pkt->stream_index == p.audio_in_idx && p.audio_out_idx >= 0) {
      ret = ProcessAudioCopy(p);
      if (ret < 0) {
        std::cerr << "[encoder-pusher] process audio failed: " << AvErr2Str(ret) << std::endl;
        return ret;
      }
      continue;
    }

    av_packet_unref(p.in_pkt);
  }

  if (ret == AVERROR_EOF) {
    ret = FlushVideo(p, running);
    if (ret < 0) {
      std::cerr << "[encoder-pusher] flush video failed: " << AvErr2Str(ret) << std::endl;
      return ret;
    }
    av_write_trailer(p.out_ctx);
    return AVERROR_EOF;
  }

  av_write_trailer(p.out_ctx);
  return ret;
}

void PushByCliFallback(const TaskConfig& cfg, const std::atomic<bool>& running) {
  const std::string target_url = BuildTargetUrl(cfg);
  std::ostringstream cmd;
  cmd << "ffmpeg -re "
      << "-f lavfi -i testsrc=size=" << cfg.width << "x" << cfg.height << ":rate=" << cfg.fps << " "
      << "-f lavfi -i sine=frequency=" << cfg.audio_freq << ":sample_rate=48000 "
      << "-c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p "
      << "-g 60 -keyint_min 60 -sc_threshold 0 "
      << "-c:a aac -b:a 128k -ar 48000 -ac 2 "
      << "-f flv \"" << target_url << "\"";

  while (running.load()) {
    int code = std::system(cmd.str().c_str());
    if (!running.load()) break;
    std::cerr << "[encoder-pusher] fallback ffmpeg exited code=" << code << ", retry in 1s" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

class TaskManager {
 public:
  bool StartTask(const TaskConfig& cfg) {
    std::lock_guard<std::mutex> lock(mu_);
    if (tasks_.count(cfg.stream_id) > 0) {
      std::cerr << "[encoder-pusher] stream already exists: " << cfg.stream_id << std::endl;
      return false;
    }

    auto task = std::make_unique<TaskState>();
    task->config = cfg;
    task->running.store(true);
    task->worker = std::thread([state = task.get()]() { RunTask(*state); });
    tasks_[cfg.stream_id] = std::move(task);
    return true;
  }

  void StopAll() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      for (auto& kv : tasks_) kv.second->running.store(false);
    }

    std::lock_guard<std::mutex> lock(mu_);
    for (auto& kv : tasks_) {
      if (kv.second->worker.joinable()) kv.second->worker.join();
    }
    tasks_.clear();
  }

  void ListTasks() {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& kv : tasks_) {
      const auto& cfg = kv.second->config;
      std::cout << "[encoder-pusher] streamId=" << cfg.stream_id
                << " target=" << BuildTargetUrl(cfg)
                << " mode=" << (cfg.input_file.empty() ? "fallback-cli-testsrc" : "frame-render-api")
                << " input=" << (cfg.input_file.empty() ? "testsrc" : cfg.input_file)
                << std::endl;
    }
  }

 private:
  struct TaskState {
    TaskConfig config;
    std::atomic<bool> running{false};
    std::thread worker;
  };

  static void RunTask(TaskState& state) {
    const auto& cfg = state.config;
    std::cout << "[encoder-pusher] start streamId=" << cfg.stream_id << std::endl;

    if (cfg.input_file.empty()) {
      PushByCliFallback(cfg, state.running);
      std::cout << "[encoder-pusher] stop streamId=" << cfg.stream_id << std::endl;
      return;
    }

    while (state.running.load()) {
      int ret = PushByFrameRenderOnce(cfg, state.running);
      if (!state.running.load()) break;

      if (ret == AVERROR_EOF) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        continue;
      }

      std::cerr << "[encoder-pusher] frame-render loop error streamId=" << cfg.stream_id
                << ", err=" << AvErr2Str(ret) << ", retry in 1s" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[encoder-pusher] stop streamId=" << cfg.stream_id << std::endl;
  }

  std::mutex mu_;
  std::map<std::string, std::unique_ptr<TaskState>> tasks_;
};

TaskConfig ParseArgs(int argc, char** argv) {
  TaskConfig cfg;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto get_val = [&](const char* name) -> std::string {
      if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + name);
      return argv[++i];
    };

    if (arg == "--stream-id") cfg.stream_id = get_val("--stream-id");
    else if (arg == "--host") cfg.host = get_val("--host");
    else if (arg == "--port") cfg.port = std::stoi(get_val("--port"));
    else if (arg == "--app") cfg.app = get_val("--app");
    else if (arg == "--input") cfg.input_file = get_val("--input");
    else if (arg == "--fps") cfg.fps = std::stoi(get_val("--fps"));
    else if (arg == "--width") cfg.width = std::stoi(get_val("--width"));
    else if (arg == "--height") cfg.height = std::stoi(get_val("--height"));
    else if (arg == "--audio-freq") cfg.audio_freq = std::stoi(get_val("--audio-freq"));
    else throw std::runtime_error("unknown arg: " + arg);
  }
  return cfg;
}
}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  av_log_set_level(AV_LOG_ERROR);

  try {
    TaskConfig cfg = ParseArgs(argc, argv);
    TaskManager manager;
    if (!manager.StartTask(cfg)) return 1;
    manager.ListTasks();

    while (g_running.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    manager.StopAll();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    std::cerr << "Usage: encoder_pusher [--stream-id stream1] [--host 127.0.0.1] [--port 1935] "
                 "[--app live] [--input /path/a.mp4] [--fps 30] [--width 1280] [--height 720] [--audio-freq 1000]"
              << std::endl;
    return 1;
  }
}
