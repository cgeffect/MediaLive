#include "frame_render_pusher.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

#include "audio_processor.h"
#include "ffmpeg_utils.h"
#include "frame_renderer.h"
#include "input_initializer.h"
#include "output_initializer.h"
#include "video_processor.h"

FrameRenderPusher::FrameRenderPusher(TaskConfig cfg) : cfg_(std::move(cfg)) {
}

int FrameRenderPusher::RunFrameRenderOnce(const std::atomic<bool> &running) {
    ApiPipeline pipeline;
    FrameRenderer renderer;

    int ret = InitInput(pipeline, cfg_);
    if (ret < 0) {
        std::cerr << "[encoder-pusher] init input failed: " << AvErr2Str(ret) << std::endl;
        return ret;
    }

    ret = InitOutput(pipeline, cfg_);
    if (ret < 0) {
        std::cerr << "[encoder-pusher] init output failed: " << AvErr2Str(ret) << std::endl;
        return ret;
    }

    while (running.load()) {
        ret = av_read_frame(pipeline.in_ctx, pipeline.in_pkt);
        if (ret < 0) {
            break;
        }

        if (pipeline.in_pkt->stream_index == pipeline.video_in_idx) {
            ret = ProcessVideoDecode(pipeline, running, renderer);
            av_packet_unref(pipeline.in_pkt);
            if (ret < 0) {
                std::cerr << "[encoder-pusher] process video failed: " << AvErr2Str(ret) << std::endl;
                return ret;
            }
            continue;
        }

        if (pipeline.in_pkt->stream_index == pipeline.audio_in_idx && pipeline.audio_out_idx >= 0) {
            ret = ProcessAudioCopy(pipeline);
            if (ret < 0) {
                std::cerr << "[encoder-pusher] process audio failed: " << AvErr2Str(ret) << std::endl;
                return ret;
            }
            continue;
        }

        av_packet_unref(pipeline.in_pkt);
    }

    if (ret == AVERROR_EOF) {
        ret = FlushVideo(pipeline, running);
        if (ret < 0) {
            std::cerr << "[encoder-pusher] flush video failed: " << AvErr2Str(ret) << std::endl;
            return ret;
        }
        av_write_trailer(pipeline.out_ctx);
        return AVERROR_EOF;
    }

    av_write_trailer(pipeline.out_ctx);
    return ret;
}

void FrameRenderPusher::RunCliFallback(const std::atomic<bool> &running) const {
    const std::string target_url = BuildTargetUrl(cfg_);
    std::ostringstream cmd;
    cmd << "ffmpeg -re "
        << "-f lavfi -i testsrc=size=" << cfg_.width << "x" << cfg_.height << ":rate=" << cfg_.fps << " "
        << "-f lavfi -i sine=frequency=" << cfg_.audio_freq << ":sample_rate=48000 "
        << "-c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p "
        << "-g 60 -keyint_min 60 -sc_threshold 0 "
        << "-c:a aac -b:a 128k -ar 48000 -ac 2 "
        << "-f flv \"" << target_url << "\"";

    while (running.load()) {
        int code = std::system(cmd.str().c_str());
        if (!running.load()) {
            break;
        }
        std::cerr << "[encoder-pusher] fallback ffmpeg exited code=" << code << ", retry in 1s" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
