//
//  RTMPPushLive.cpp
//  CGLive
//
//  Created by Jason on 2022/5/27.
//

#include "RTMPPushLive.h"
#include "MogicDefines.h"

extern "C" {
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
};
#include <string>
#include <iostream>

namespace mogic {

int RTMPPushLive::interrupt_cb(void *ctx) { // 超时回调函数
    return 0;
}

RTMPPushLive::RTMPPushLive() {
    av_log_set_level(AV_LOG_DEBUG);
}

RTMPPushLive::~RTMPPushLive() {
    destroy();
}

int RTMPPushLive::initRTMP(int srcWidth, int srcHeight, AVPixelFormat srcFormat, const char *outPath, const char *audioPath) {
    videoInfo.srcWidth = srcWidth;
    videoInfo.srcHeight = srcHeight;
    videoInfo.srcPixFmt = srcFormat;

    videoInfo.dstWidth = (srcWidth >> 1) << 1;
    videoInfo.dstHeight = (srcHeight >> 1) << 1;
    videoInfo.dstPixFmt = AV_PIX_FMT_YUV420P;
    videoInfo.outPath = outPath;
    videoInfo.dstFps = 25;
    videoInfo.bitRate = videoInfo.dstWidth * videoInfo.dstHeight * 3;

    initContext();
    if (audioPath) {
        setAudioLive(audioPath);
    }

    //打开连接通道
    if (!(ofmtCtx->oformat->flags & AVFMT_NOFILE)) {
        AVDictionary *format_opts = NULL;
        av_dict_set(&format_opts, "rw_timeout", "1000000", 0); //设置超时时间,单位mcs

        AVIOInterruptCB int_cb = {interrupt_cb, this};
        ofmtCtx->interrupt_callback = int_cb;
        int ret = avio_open2(&ofmtCtx->pb, videoInfo.outPath, AVIO_FLAG_WRITE, &ofmtCtx->interrupt_callback, &format_opts);
        if (ret < 0) {
            char tmp[AV_ERROR_MAX_STRING_SIZE] = {0};
            char *err = av_make_error_string(tmp, AV_ERROR_MAX_STRING_SIZE, ret);
            MOGIC_DLOG("Could not open '%s': %s\n", outPath, err);
            return -1;
        }
    }

    AVDictionary *opt = nullptr;
    int result = avformat_write_header(ofmtCtx, &opt);
    if (result < 0) {
        MOGIC_DLOG("avformat_write_header fail");
        ;
    }
    isConnected = true;
    return 0;
}

void RTMPPushLive::setAudioLive(const char *outPath) {
    hasAudio = true;
    //找到编码格式
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVSampleFormat sampleFmt = AV_SAMPLE_FMT_NONE;
    if (strcmp(codec->name, "aac") == 0) {
        sampleFmt = AV_SAMPLE_FMT_FLTP;
    } else if (strcmp(codec->name, "libfdk_aac") == 0) {
        sampleFmt = AV_SAMPLE_FMT_S16;
    } else {
        sampleFmt = AV_SAMPLE_FMT_FLTP;
    }
    demuxer = new FFAVDemuxer();
    demuxer->loadResource(outPath);
    audioStream = initAudioStream(44100, 2, 6400, demuxer->aStream);
    if (!audioStream) {
        MOGIC_DLOG("initAudioStream fail");
    }
}

int RTMPPushLive::initContext() {
    avformat_network_init();
    // open live
    int result = avformat_alloc_output_context2(&ofmtCtx, nullptr, "flv", videoInfo.outPath);
    if (result < 0) {
        MOGIC_DLOG("avformat_alloc_output_context2 fail");
        return -1;
    }
    videoStream = initVideoStream();
    if (!videoStream) {
        return -1;
    }

    //颜色转换
    if (videoInfo.srcPixFmt != videoInfo.dstPixFmt) {
        yuv420pFrame = av_frame_alloc();
        MOGIC_CHECK_ELOG(yuv420pFrame == nullptr, MOGIC_FFENCODER_INIT_ERROR, "FFVideoRecorder::initRecord av_frame_alloc fail.")

        yuv420pFrame->width = h264CodecCtx->width;
        yuv420pFrame->height = h264CodecCtx->height;
        yuv420pFrame->format = h264CodecCtx->pix_fmt;
        int bufferSize =
            av_image_get_buffer_size(h264CodecCtx->pix_fmt, h264CodecCtx->width, h264CodecCtx->height, 1);
        yuv420pBuffer = (uint8_t *)av_malloc(bufferSize);
        av_image_fill_arrays(yuv420pFrame->data, yuv420pFrame->linesize, yuv420pBuffer, h264CodecCtx->pix_fmt,
                             h264CodecCtx->width, h264CodecCtx->height, 1);

        swsContext = sws_getContext(videoInfo.srcWidth, videoInfo.srcHeight, videoInfo.srcPixFmt,
                                    videoInfo.dstWidth, videoInfo.dstHeight, videoInfo.dstPixFmt,
                                    SWS_LANCZOS | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);
        // videoInfo.distHeight, videoInfo.distPixFmt, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        MOGIC_CHECK_ELOG(swsContext == nullptr, MOGIC_FFENCODER_INIT_ERROR, "RTMPPushLive sws_getcontext error")
    }

    packet = av_packet_alloc();
    MOGIC_CHECK_ELOG(packet == nullptr, MOGIC_FFENCODER_INIT_ERROR, "RTMPPushLive av_packet_alloc fail.")

    //微妙
    startTime = av_gettime();

    return 0;
}

AVStream *RTMPPushLive::initVideoStream() {
    // open codec
    AVCodec *h264Codec = const_cast<AVCodec *>(avcodec_find_encoder(AV_CODEC_ID_H264));
    AVCodecContext *codecCtx = avcodec_alloc_context3(h264Codec);
    codecCtx->codec_id = h264Codec->id;
    codecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    codecCtx->pix_fmt = videoInfo.dstPixFmt;
    codecCtx->width = videoInfo.dstWidth;
    codecCtx->height = videoInfo.dstHeight;
    codecCtx->time_base.num = 1;
    codecCtx->time_base.den = videoInfo.dstFps;
    codecCtx->bit_rate = videoInfo.bitRate;
    codecCtx->gop_size = 5;
    codecCtx->thread_count = 8;
    codecCtx->thread_type = FF_THREAD_FRAME;
    codecCtx->max_b_frames = 0;

    if (h264Codec->id == AV_CODEC_ID_H264) {
        int ret = av_opt_set(codecCtx->priv_data, "preset", "ultrafast", 0);
        if (ret != 0) {
            MOGIC_WARN("av_opt_set preset failed");
        }
        ret = av_opt_set(codecCtx->priv_data, "profile", "baseline", 0);
        if (ret != 0) {
            MOGIC_WARN("av_opt_set profile failed");
        }
        ret = av_opt_set(codecCtx->priv_data, "tune", "zerolatency", 0);
        if (ret != 0) {
            MOGIC_WARN("av_opt_set tune failed");
        }
    }
    //沒有它們，編碼器可能會在發送到容器的數據中添加元數據。對於AAC它是ADTS標題，對於H264它是SPS和PPS數據。
    if (ofmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    int result = avcodec_open2(codecCtx, h264Codec, nullptr);
    if (result < 0) {
        MOGIC_DLOG("open codec fail");
    }

    // open stream, 这一步之后会创建一个流, 这个流会保存在ofmtCtx
    // ffmpeg 推流流程, 1. AVFormatContext, 2. AVFormatContext里面包含具体的要推送的流数据,
    AVStream *stream = avformat_new_stream(ofmtCtx, nullptr);
    //把codecCtx赋值给stream, codecCtx最终是要给AVStream用的
    result = avcodec_parameters_from_context(stream->codecpar, codecCtx);
    if (result < 0) {
        return nullptr;
    }
    stream->id = ofmtCtx->nb_streams - 1; // index的值由AVFormatContext设置, 所引从0开始一次递增
    h264CodecCtx = codecCtx;
    return stream;
}

AVStream *RTMPPushLive::initAudioStream(int audioSampleRate, int audioChannels, int audioBitRate, AVStream *src) {
    inStream = src;
    AVCodec *aacCodec = avcodec_find_encoder(src->codecpar->codec_id);
    //设置codec参数
    AVStream *st = avformat_new_stream(ofmtCtx, NULL);

    AVCodecContext *codecCtx = avcodec_alloc_context3(aacCodec);
    int ret = avcodec_parameters_to_context(codecCtx, src->codecpar);
    if (ret < 0) {
        return nullptr;
    }
    codecCtx->codec_tag = 0;

    if (ofmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_parameters_from_context(st->codecpar, codecCtx);
    if (ret < 0) {
        MOGIC_DLOG("Failed to copy codec context to out_stream codecpar context");
    }

    aacCodecCtx = codecCtx;
    return st;
}

// byteData是rgba数据
int RTMPPushLive::encodeByteData(uint8_t *byteData, uint32_t frameIndex) {
    if (!isConnected) {
        MOGIC_DLOG("未连接 %s", videoInfo.outPath);
        return -1;
    }
    int64_t ptsTime = frameIndex * (1000 / videoInfo.dstFps) * 1000;
    int64_t nowTime = av_gettime() - startTime;
    // std::cout << "ptsTime: " << ptsTime << std::endl;
    if (ptsTime > nowTime) {
        av_usleep((int)(ptsTime - nowTime));
        MOGIC_DLOG("sleep: %d", ptsTime - nowTime);
    }
    if (hasAudio) {
        pushAudio(frameIndex, nowTime);
    }

    if (byteData != nullptr) {
        if (videoInfo.srcPixFmt != videoInfo.dstPixFmt && swsContext != nullptr) {
            uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
            int lineSize[AV_NUM_DATA_POINTERS] = {0};
            data[0] = byteData;
            lineSize[0] = videoInfo.dstWidth * 4;
            sws_scale(swsContext, data, lineSize, 0, videoInfo.dstHeight, yuv420pFrame->data, yuv420pFrame->linesize);
        } else {
            MOGIC_ERROR("RTMPPushLive::encodeByteData frameIndex=%d sws_scale error", frameIndex);
            return -1;
        }

        //设置 pts, pts的值为什么是index
        yuv420pFrame->pts = frameIndex;
        if (encodeFrame(yuv420pFrame) < 0) {
            return -1;
        }
    }

    return -1;
}

void RTMPPushLive::pushAudio(int frameIndex, int64_t nowTime) {
    while (true) {
        AVPacket *pkt = demuxer->demuxerPacket();
        if (pkt->pts == AV_NOPTS_VALUE) {
            // Write PTS
            AVRational time_base1 = audioStream->time_base;
            // Duration between 2 frames (us)
            int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(audioStream->r_frame_rate);
            // Parameters
            pkt->pts = (double)(frameIndex * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
            pkt->dts = pkt->pts;
            pkt->duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
        }

        AVStream *outStream = audioStream;
        pkt->pts = av_rescale_q_rnd(pkt->pts, inStream->time_base, outStream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts, inStream->time_base, outStream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration, inStream->time_base, outStream->time_base);
        pkt->pos = -1;

        pkt->stream_index = audioStream->index;
        int64_t ptsMs = pkt->pts * av_q2d(audioStream->time_base) * 1000;
        // av_packet_rescale_ts(pkt, aacCodecCtx->time_base, audioStream->time_base);
        av_interleaved_write_frame(ofmtCtx, pkt);

        audioIndex++;
        MOGIC_DLOG("audio index: %d ptsMs: %lld", audioIndex, ptsMs);
        if (ptsMs * 1000 > nowTime) {
            break;
        }
    }
}
//推流的几个关键点
// AVFormatContext 输出上下文
// AVStream, 由AVFormatContext创建
// AVPacket, 具体要推流的压缩数据
int RTMPPushLive::encodeFrame(AVFrame *srcFrame) {
    if (srcFrame == nullptr) {
        return 0;
    }
    int ret = 0;
    ret = avcodec_send_frame(h264CodecCtx, srcFrame);
    if (ret == AVERROR(EAGAIN)) {
        //数据不够, 继续送
        return 0;
    }
    MOGIC_CHECK_ELOG(ret < 0, MOGIC_FFENCODER_ENCODE_ERROR, "RTMPPushLive avcodec_send_frame fail. ret=%d", ret)

    while (true) {
        ret = avcodec_receive_packet(h264CodecCtx, packet);
        if (ret == AVERROR(EAGAIN)) {
            //数据不够, 继续送
            return 0;
        }

        if (ret == AVERROR_EOF) {
            return ret;
        }

        if (ret < 0) {
            MOGIC_ERROR("RTMPPushLive avcodec_receive_packet fail. ret=%d", result);
            return MOGIC_FFENCODER_ENCODE_ERROR;
        }

        packet->stream_index = videoStream->index;
        // h264CodecCtx->time_base=1/25, videoStream->time_base=1/1000
        // pts的值会从frame的pts继承过来, frame设置的是索引值, av_packet_rescale_ts是把索引值转化为pts
        av_packet_rescale_ts(packet, h264CodecCtx->time_base, videoStream->time_base);

        auto ptsMs = packet->pts * av_q2d(videoStream->time_base) * 1000;
        auto dtsMs = packet->dts * av_q2d(videoStream->time_base) * 1000;
        MOGIC_DLOG("video index: %d ptsMs: %d", srcFrame->pts, ptsMs);
        // m_avPacket.pos = -1;
        av_interleaved_write_frame(ofmtCtx, packet);
        // av_packet_unref(m_avPacket);
    }
    return ret;
}

int RTMPPushLive::encodeTail() {
    int result = encodeFrame(nullptr);
    if (result >= 0) {
        av_write_trailer(ofmtCtx);
    }

    destroy();

    return 0;
}

void RTMPPushLive::destroy() {
    if (yuv420pFrame != nullptr) {
        av_frame_free(&yuv420pFrame);
        yuv420pFrame = nullptr;
    }
    if (yuv420pBuffer != nullptr) {
        av_free(yuv420pBuffer);
        yuv420pBuffer = nullptr;
    }
    if (packet != nullptr) {
        av_packet_free(&packet);
        packet = nullptr;
    }
    if (h264CodecCtx != nullptr) {
        avcodec_close(h264CodecCtx);
        avcodec_free_context(&h264CodecCtx);
        h264CodecCtx = nullptr;
    }
    if (ofmtCtx != nullptr) {
        avio_close(ofmtCtx->pb);
        avformat_free_context(ofmtCtx);
        ofmtCtx = nullptr;
    }
    if (swsContext != nullptr) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    if (bsf_ctx) {
        av_bsf_free(&bsf_ctx);
    }
}
} // namespace mogic
