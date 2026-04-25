#include "pipeline_context.h"

ApiPipeline::~ApiPipeline() {
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }
    if (dec_frame) {
        av_frame_free(&dec_frame);
    }
    if (enc_frame) {
        av_frame_free(&enc_frame);
    }
    if (in_pkt) {
        av_packet_free(&in_pkt);
    }
    if (out_pkt) {
        av_packet_free(&out_pkt);
    }
    if (vdec_ctx) {
        avcodec_free_context(&vdec_ctx);
    }
    if (venc_ctx) {
        avcodec_free_context(&venc_ctx);
    }
    if (out_ctx) {
        if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&out_ctx->pb);
        }
        avformat_free_context(out_ctx);
    }
    if (in_ctx) {
        avformat_close_input(&in_ctx);
    }
}
