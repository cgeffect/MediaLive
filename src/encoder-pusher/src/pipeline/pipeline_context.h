#pragma once

#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

struct ApiPipeline {
    AVFormatContext *in_ctx = nullptr;
    AVFormatContext *out_ctx = nullptr;

    int video_in_idx = -1;
    int audio_in_idx = -1;
    int video_out_idx = -1;
    int audio_out_idx = -1;

    AVCodecContext *vdec_ctx = nullptr;
    AVCodecContext *venc_ctx = nullptr;

    AVFrame *dec_frame = nullptr;
    AVFrame *enc_frame = nullptr;
    AVPacket *in_pkt = nullptr;
    AVPacket *out_pkt = nullptr;

    SwsContext *sws_ctx = nullptr;

    int64_t frame_index = 0;
    int64_t wall_start_us = 0;
    int64_t media_start_us = AV_NOPTS_VALUE;

    ~ApiPipeline();
};
