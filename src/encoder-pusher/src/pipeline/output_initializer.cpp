#include "output_initializer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

#include "task_config.h"

int InitOutput(ApiPipeline &pipeline, const TaskConfig &cfg) {
    const std::string target_url = BuildTargetUrl(cfg);
    int ret = avformat_alloc_output_context2(&pipeline.out_ctx, nullptr, "flv", target_url.c_str());
    if (ret < 0 || !pipeline.out_ctx) {
        return ret < 0 ? ret : AVERROR_UNKNOWN;
    }

    AVStream *vin = pipeline.in_ctx->streams[pipeline.video_in_idx];
    AVStream *vout = avformat_new_stream(pipeline.out_ctx, nullptr);
    if (!vout) {
        return AVERROR(ENOMEM);
    }
    pipeline.video_out_idx = vout->index;

    const AVCodec *venc = avcodec_find_encoder_by_name("libx264");
    if (!venc) {
        venc = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!venc) {
        return AVERROR_ENCODER_NOT_FOUND;
    }

    pipeline.venc_ctx = avcodec_alloc_context3(venc);
    if (!pipeline.venc_ctx) {
        return AVERROR(ENOMEM);
    }

    pipeline.venc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    pipeline.venc_ctx->codec_id = venc->id;
    pipeline.venc_ctx->width = pipeline.vdec_ctx->width;
    pipeline.venc_ctx->height = pipeline.vdec_ctx->height;
    pipeline.venc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    pipeline.venc_ctx->time_base = av_inv_q(vin->r_frame_rate.num > 0 ? vin->r_frame_rate : AVRational{cfg.fps, 1});
    if (pipeline.venc_ctx->time_base.num <= 0 || pipeline.venc_ctx->time_base.den <= 0) {
        pipeline.venc_ctx->time_base = AVRational{1, cfg.fps};
    }
    pipeline.venc_ctx->framerate = av_inv_q(pipeline.venc_ctx->time_base);
    pipeline.venc_ctx->gop_size = cfg.fps * 2;
    pipeline.venc_ctx->max_b_frames = 0;
    pipeline.venc_ctx->bit_rate = 2000 * 1000;

    av_opt_set(pipeline.venc_ctx->priv_data, "preset", "veryfast", 0);
    av_opt_set(pipeline.venc_ctx->priv_data, "tune", "zerolatency", 0);

    if (pipeline.out_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        pipeline.venc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(pipeline.venc_ctx, venc, nullptr);
    if (ret < 0) {
        return ret;
    }

    ret = avcodec_parameters_from_context(vout->codecpar, pipeline.venc_ctx);
    if (ret < 0) {
        return ret;
    }
    vout->time_base = pipeline.venc_ctx->time_base;

    if (pipeline.audio_in_idx >= 0) {
        AVStream *ain = pipeline.in_ctx->streams[pipeline.audio_in_idx];
        if (ain->codecpar->codec_id == AV_CODEC_ID_AAC) {
            AVStream *aout = avformat_new_stream(pipeline.out_ctx, nullptr);
            if (!aout) {
                return AVERROR(ENOMEM);
            }
            pipeline.audio_out_idx = aout->index;
            ret = avcodec_parameters_copy(aout->codecpar, ain->codecpar);
            if (ret < 0) {
                return ret;
            }
            aout->codecpar->codec_tag = 0;
            aout->time_base = ain->time_base;
        }
    }

    if (!(pipeline.out_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&pipeline.out_ctx->pb, target_url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            return ret;
        }
    }

    ret = avformat_write_header(pipeline.out_ctx, nullptr);
    if (ret < 0) {
        return ret;
    }

    pipeline.dec_frame = av_frame_alloc();
    pipeline.enc_frame = av_frame_alloc();
    pipeline.in_pkt = av_packet_alloc();
    pipeline.out_pkt = av_packet_alloc();
    if (!pipeline.dec_frame || !pipeline.enc_frame || !pipeline.in_pkt || !pipeline.out_pkt) {
        return AVERROR(ENOMEM);
    }

    pipeline.enc_frame->format = pipeline.venc_ctx->pix_fmt;
    pipeline.enc_frame->width = pipeline.venc_ctx->width;
    pipeline.enc_frame->height = pipeline.venc_ctx->height;
    ret = av_frame_get_buffer(pipeline.enc_frame, 32);
    if (ret < 0) {
        return ret;
    }

    pipeline.wall_start_us = av_gettime_relative();
    pipeline.media_start_us = AV_NOPTS_VALUE;

    return 0;
}
