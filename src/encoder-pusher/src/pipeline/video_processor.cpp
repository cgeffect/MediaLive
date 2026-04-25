#include "video_processor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

#include "ffmpeg_utils.h"

namespace {

int ConvertOrCopyToEncoderFrame(ApiPipeline &pipeline, AVFrame *src, AVFrame *&out) {
    if (src->format == pipeline.venc_ctx->pix_fmt && src->width == pipeline.venc_ctx->width && src->height == pipeline.venc_ctx->height) {
        out = src;
        return 0;
    }

    int ret = av_frame_make_writable(pipeline.enc_frame);
    if (ret < 0) {
        return ret;
    }

    if (!pipeline.sws_ctx) {
        pipeline.sws_ctx = sws_getContext(src->width,
                                          src->height,
                                          static_cast<AVPixelFormat>(src->format),
                                          pipeline.venc_ctx->width,
                                          pipeline.venc_ctx->height,
                                          pipeline.venc_ctx->pix_fmt,
                                          SWS_BILINEAR,
                                          nullptr,
                                          nullptr,
                                          nullptr);
        if (!pipeline.sws_ctx) {
            return AVERROR(EINVAL);
        }
    }

    sws_scale(pipeline.sws_ctx,
              src->data,
              src->linesize,
              0,
              src->height,
              pipeline.enc_frame->data,
              pipeline.enc_frame->linesize);
    pipeline.enc_frame->pts = src->pts;
    out = pipeline.enc_frame;
    return 0;
}

int WriteVideoPacket(ApiPipeline &pipeline) {
    AVStream *vout = pipeline.out_ctx->streams[pipeline.video_out_idx];
    pipeline.out_pkt->stream_index = pipeline.video_out_idx;

    av_packet_rescale_ts(pipeline.out_pkt, pipeline.venc_ctx->time_base, vout->time_base);
    PaceByTimestamp(pipeline, pipeline.out_pkt->dts, vout->time_base);

    int ret = av_interleaved_write_frame(pipeline.out_ctx, pipeline.out_pkt);
    av_packet_unref(pipeline.out_pkt);
    return ret;
}

} // namespace

int ProcessVideoDecode(ApiPipeline &pipeline, const std::atomic<bool> &running, const FrameRenderer &renderer) {
    int ret = avcodec_send_packet(pipeline.vdec_ctx, pipeline.in_pkt);
    if (ret < 0) {
        return ret;
    }

    while (running.load()) {
        ret = avcodec_receive_frame(pipeline.vdec_ctx, pipeline.dec_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        if (ret < 0) {
            return ret;
        }

        int64_t src_pts = (pipeline.dec_frame->best_effort_timestamp != AV_NOPTS_VALUE) ? pipeline.dec_frame->best_effort_timestamp : pipeline.dec_frame->pts;
        if (src_pts == AV_NOPTS_VALUE) {
            src_pts = pipeline.frame_index;
        }

        pipeline.dec_frame->pts = av_rescale_q(src_pts,
                                               pipeline.in_ctx->streams[pipeline.video_in_idx]->time_base,
                                               pipeline.venc_ctx->time_base);

        AVFrame *render_frame = nullptr;
        ret = ConvertOrCopyToEncoderFrame(pipeline, pipeline.dec_frame, render_frame);
        if (ret < 0) {
            av_frame_unref(pipeline.dec_frame);
            return ret;
        }

        renderer.Apply(render_frame, pipeline.frame_index++);

        ret = avcodec_send_frame(pipeline.venc_ctx, render_frame);
        av_frame_unref(pipeline.dec_frame);
        if (ret < 0) {
            return ret;
        }

        while (running.load()) {
            ret = avcodec_receive_packet(pipeline.venc_ctx, pipeline.out_pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                return ret;
            }

            ret = WriteVideoPacket(pipeline);
            if (ret < 0) {
                return ret;
            }
        }
    }

    return 0;
}

int FlushVideo(ApiPipeline &pipeline, const std::atomic<bool> &running) {
    int ret = avcodec_send_frame(pipeline.venc_ctx, nullptr);
    if (ret < 0) {
        return ret;
    }

    while (running.load()) {
        ret = avcodec_receive_packet(pipeline.venc_ctx, pipeline.out_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        if (ret < 0) {
            return ret;
        }

        ret = WriteVideoPacket(pipeline);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}
