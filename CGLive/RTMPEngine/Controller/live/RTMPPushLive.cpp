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

RTMPPushLive::RTMPPushLive() {
}

RTMPPushLive::~RTMPPushLive() {
    destroy();
}

int RTMPPushLive::initRTMP(int srcWidth, int srcHeight, AVPixelFormat srcFormat, const char *outPath) {
    videoInfo.srcWidth = srcWidth;
    videoInfo.srcHeight = srcHeight;
    videoInfo.srcPixFmt = srcFormat;

    videoInfo.dstWidth = (srcWidth >> 1) << 1;
    videoInfo.dstHeight = (srcHeight >> 1) << 1;
    videoInfo.dstPixFmt = AV_PIX_FMT_YUV420P;
    videoInfo.outPath = outPath;
    videoInfo.dstFps = 25;
    videoInfo.bitRate = videoInfo.dstWidth * videoInfo.dstHeight * 3;

    initCodecCtx();

    return 0;
}
void RTMPPushLive::setAudioLive(const char *outPath) {
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
    /*
        //audio
    AVCodec *aacCodec = const_cast<AVCodec *>(avcodec_find_encoder(AV_CODEC_ID_AAC));
    AVCodecContext *aacCodecCtx = nullptr;
    aacCodecCtx = avcodec_alloc_context3(aacCodec);
    aacCodecCtx->codec_id = aacCodec->id;
    aacCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    aacCodecCtx->pix_fmt = AV_SAMPLE_FMT_FLTP;
    aacCodecCtx->
    AVStream *aStream = avformat_new_stream(ofmtCtx, nullptr);
    */
}

int RTMPPushLive::initCodecCtx() {
    avformat_network_init();
    int result = -1;

    // open live
    result = avformat_alloc_output_context2(&ofmtCtx, nullptr, "flv", videoInfo.outPath);

    // open codec
    AVCodec *h264Codec = const_cast<AVCodec *>(avcodec_find_encoder(AV_CODEC_ID_H264));
    codecCtx = avcodec_alloc_context3(h264Codec);
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
    // av_opt_set(codecCtx->priv_data, "muxdelay", "1", 0);
    // av_opt_set(codecCtx->priv_data, "crf", "16", 0);

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
    if (ofmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    result = avcodec_open2(codecCtx, h264Codec, nullptr);
    if (result < 0) {
        std::cout << "open codec fail" << std::endl;
    }

    // open stream, 这一步之后会创建一个流, 这个流会保存在ofmtCtx
    //ffmpeg 推流流程, 1. AVFormatContext, 2. AVFormatContext里面包含具体的要推送的流数据,
    newStream = avformat_new_stream(ofmtCtx, nullptr);
    result = avcodec_parameters_from_context(newStream->codecpar, codecCtx);

    if (!(ofmtCtx->oformat->flags & AVFMT_NOFILE)) {
        result = avio_open(&ofmtCtx->pb, videoInfo.outPath, AVIO_FLAG_READ_WRITE);
    }

    if (codecCtx->codec_id == AV_CODEC_ID_H264) {
        AVDictionary *opt = nullptr;
        result = avformat_write_header(ofmtCtx, &opt);
    }

    if (videoInfo.srcPixFmt != videoInfo.dstPixFmt) {
        yuv420pFrame = av_frame_alloc();
        MOGIC_CHECK_ELOG(yuv420pFrame == nullptr, MOGIC_FFENCODER_INIT_ERROR, "FFVideoRecorder::initRecord av_frame_alloc fail.")

        yuv420pFrame->width = codecCtx->width;
        yuv420pFrame->height = codecCtx->height;
        yuv420pFrame->format = codecCtx->pix_fmt;
        int bufferSize =
            av_image_get_buffer_size(codecCtx->pix_fmt, codecCtx->width, codecCtx->height, 1);
        yuv420pBuffer = (uint8_t *)av_malloc(bufferSize);
        av_image_fill_arrays(yuv420pFrame->data, yuv420pFrame->linesize, yuv420pBuffer, codecCtx->pix_fmt,
                             codecCtx->width, codecCtx->height, 1);

        swsContext =
            sws_getContext(videoInfo.srcWidth, videoInfo.srcHeight, videoInfo.srcPixFmt,
                           videoInfo.dstWidth, videoInfo.dstHeight, videoInfo.dstPixFmt,
                           SWS_LANCZOS | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);
        // videoInfo.distHeight, videoInfo.distPixFmt, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        MOGIC_CHECK_ELOG(swsContext == nullptr, MOGIC_FFENCODER_INIT_ERROR, "FFVideoRecorder::initRecord sws_getcontext error")
    }

    packet = av_packet_alloc();
    MOGIC_CHECK_ELOG(packet == nullptr, MOGIC_FFENCODER_INIT_ERROR, "FFVideoRecorder::initRecord av_packet_alloc fail.")

    if (result < 0) {
        destroy();
    }
    //微妙
    startTime = av_gettime();

    return result;
}

int RTMPPushLive::encodeByteData(uint8_t *byteData, uint32_t frameIndex) {
    AVRational timeBase = codecCtx->time_base;
    AVRational timeBaseQ = {1, AV_TIME_BASE};
    int64_t ptsTime = frameIndex * (1000 / videoInfo.dstFps) * 1000;
    int64_t nowTime = av_gettime() - startTime;
    // std::cout << "ptsTime: " << ptsTime << std::endl;
    if (ptsTime > nowTime) {
        // av_usleep((int)(ptsTime - nowTime));
        // std::cout << ptsTime - nowTime << std::endl;
    }

    if (byteData == nullptr) {
        return -1;
    }
    if (videoInfo.srcPixFmt != videoInfo.dstPixFmt) {
        if (swsContext != nullptr) {
            uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
            int lineSize[AV_NUM_DATA_POINTERS] = {0};
            data[0] = byteData;
            lineSize[0] = videoInfo.dstWidth * 4;
            sws_scale(swsContext, data, lineSize, 0, videoInfo.dstHeight, yuv420pFrame->data, yuv420pFrame->linesize);
        } else {
            MOGIC_ERROR("FFVideoRecorder::encodeByteData frameIndex=%d sws_scale error", frameIndex);
            return -1;
        }
    }

    //设置 pts, pts的值为什么是index
    yuv420pFrame->pts = frameIndex;
    if (encodeFrame(yuv420pFrame) < 0) {
        return -1;
    }
    return -1;
}

int RTMPPushLive::encodeFrame(AVFrame *srcFrame) {
    int ret = 0;
    ret = avcodec_send_frame(codecCtx, srcFrame);
    if (ret == AVERROR(EAGAIN)) {
        //数据不够, 继续送
        return 0;
    }
    MOGIC_CHECK_ELOG(ret < 0, MOGIC_FFENCODER_ENCODE_ERROR, "FFVideoRecorder::EncodeFrame avcodec_send_frame fail. ret=%d", ret)

    while (true) {
        ret = avcodec_receive_packet(codecCtx, packet);
        if (ret == AVERROR(EAGAIN)) {
            //数据不够, 继续送
            return 0;
        }

        if (ret == AVERROR_EOF) {
            return ret;
        }

        if (ret < 0) {
            MOGIC_ERROR("FFVideoRecorder::EncodeFrame avcodec_receive_packet fail. ret=%d", result);
            return MOGIC_FFENCODER_ENCODE_ERROR;
        }

        packet->stream_index = newStream->index;
        av_packet_rescale_ts(packet, codecCtx->time_base, newStream->time_base);

        auto ptsMs = packet->pts * av_q2d(newStream->time_base) * 1000;
        auto dtsMs = packet->dts * av_q2d(newStream->time_base) * 1000;
        // std::cout << "ptsMs: " << ptsMs << std::endl;
        // m_avPacket.pos = -1;
        av_interleaved_write_frame(ofmtCtx, packet);
        //        av_write_frame(ofmtCtx, packet);
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
    if (codecCtx != nullptr) {
        avcodec_close(codecCtx);
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
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
}
} // namespace mogic
