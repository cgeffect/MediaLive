#include "input_initializer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int InitInput(ApiPipeline &pipeline, const TaskConfig &cfg) {
    int ret = avformat_open_input(&pipeline.in_ctx, cfg.input_file.c_str(), nullptr, nullptr);
    if (ret < 0) {
        return ret;
    }

    ret = avformat_find_stream_info(pipeline.in_ctx, nullptr);
    if (ret < 0) {
        return ret;
    }

    pipeline.video_in_idx = av_find_best_stream(pipeline.in_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (pipeline.video_in_idx < 0) {
        return pipeline.video_in_idx;
    }
    pipeline.audio_in_idx = av_find_best_stream(pipeline.in_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    AVStream *vin = pipeline.in_ctx->streams[pipeline.video_in_idx];
    const AVCodec *vdec = avcodec_find_decoder(vin->codecpar->codec_id);
    if (!vdec) {
        return AVERROR_DECODER_NOT_FOUND;
    }

    pipeline.vdec_ctx = avcodec_alloc_context3(vdec);
    if (!pipeline.vdec_ctx) {
        return AVERROR(ENOMEM);
    }

    ret = avcodec_parameters_to_context(pipeline.vdec_ctx, vin->codecpar);
    if (ret < 0) {
        return ret;
    }

    ret = avcodec_open2(pipeline.vdec_ctx, vdec, nullptr);
    if (ret < 0) {
        return ret;
    }

    return 0;
}
