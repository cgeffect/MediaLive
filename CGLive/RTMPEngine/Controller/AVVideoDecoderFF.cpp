//
//  AVVideoDecoder.cpp
//  FFmpegiOS
//
//  Created by Jason on 2020/11/11.
//  Copyright © 2020 Jason. All rights reserved.
//

#include "AVVideoDecoderFF.h"
#include <iostream>
#include <pthread.h>

//http://blog.tubumu.com/2018/10/28/ffmpeg-and-videotoolbox-03/
//https://zhuanlan.zhihu.com/p/56702862
using namespace std;
static char err_buf[128] = {0};
static char* av_get_err(int errnum)
{
    av_strerror(errnum, err_buf, 128);
    return err_buf;
}

#pragma mark - C Function
static enum AVPixelFormat hw_pix_fmt;
static AVBufferRef *hw_device_ctx = NULL;
static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type) {
    int err = av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0);
    if (err < 0) {
        printf("Failed to create specified HW device.\n");
        return err;
    }
    //此函数用于设置硬解码的设备缓冲区引用，当此参数不为NULL时，解码将使用硬解码
    //av_buffer_ref用于创建设备缓冲区
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return err;
}
static enum AVPixelFormat get_hw_format(AVCodecContext * ctx, const enum AVPixelFormat * pix_fmts) {
    const enum AVPixelFormat * p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt) {
            return *p;
        }
    }
    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}
const struct AVHWAccel *hwAccel = NULL;
static void initHwAccel(AVCodecContext *avcodec_context, AVCodec *avcodec) {
    //根据设备类型
    const char *codecName = av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_VIDEOTOOLBOX);
    enum AVHWDeviceType type = av_hwdevice_find_type_by_name(codecName);
    if (type != AV_HWDEVICE_TYPE_VIDEOTOOLBOX) {
        printf("%s: Not find hardware codec\n",__func__);
        return;
    }

    //获取匹配的输出格式
    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(avcodec, i);
        if (!config) {
            fprintf(stderr, "Decoder %s does not support device type %s.\n",
                    avcodec->name, av_hwdevice_get_type_name(type));
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == type) {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }
    
    //此函数用于获取硬解码对应的像素格式，比如videotoolbox就是AV_PIX_FORMAT_VIDEOTOOLBOX
    avcodec_context->get_format = get_hw_format;
    
    //    AVHWDeviceType t = av_hwdevice_iterate_types(type);
    int ret = hw_decoder_init(avcodec_context, type);
    if (ret < 0){
        printf("hw_decoder_init faliture\n");
        return;
    }
    hwAccel = avcodec_context->hwaccel;
//    void *ctx = avcodec_context->hwaccel_context;
//    获取到这个硬件设备的限制，如最大、最小长宽和目标像素格式
//    av_hwdevice_get_hwframe_constraints(AVBufferRef *ref, const void *hwconfig)
//    hw_enable = true;
}

AVVideoDecoderFF::AVVideoDecoderFF() {
    
}

AVVideoDecoderFF::~AVVideoDecoderFF() {
    printf("dealloc");
}

void AVVideoDecoderFF::loadResource(const char *path) {
    filePath = path;
    av_demuxer.ffmpeg_demux_(path);
    avformat_context = av_demuxer.fmt_ctx;
    
    // 打印流信息到控制台
    printf("=================================\n");
    av_dump_format(avformat_context, 0, path, 0);
    fflush(stderr);
    printf("=================================\n");

    //AVFormatContext描述了一个媒体文件或媒体流的构成和基本信息

//    //第三步：读取一部分数据获取视频信息, 该函数会有一定的延迟
//    int avformat_find_stream_info_result = avformat_find_stream_info(avformat_context, NULL);
//    if (avformat_find_stream_info_result < 0) {
//        printf("avformat_find_stream_info fail");
//    }
    
    //3、查找视频流, 并且找到该流对应的解码器
    video_stream_index = av_find_best_stream(avformat_context, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    if (video_stream_index < 0) {
        printf("av_find_best_stream faliture\n");
        return;
    }
    audio_stream_index = av_find_best_stream(avformat_context, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_codec, 0);
    
    video_stream = avformat_context->streams[video_stream_index];// 音频流、视频流、字幕流
    avcodec_context = avcodec_alloc_context3(video_codec);
    
    //开启多线程解码
//    avcodec_context->thread_count = 4;
    //丢弃哪些帧不解码
//    avcodec_context->skip_frame = AVDISCARD_DEFAULT;
    //丢弃哪些帧不解码
//    video_stream->discard = AVDISCARD_DEFAULT;
    
    if (!avcodec_context){
        printf("avcodec_alloc_context3 faliture\n");
        return;
    }
       
    //Copy codec parameters from input stream to output codec context
    AVCodecParameters *codecParams = avformat_context->streams[video_stream_index]->codecpar;
    int ret = avcodec_parameters_to_context(avcodec_context, codecParams);
    if (ret < 0){
        printf("avcodec_parameters_to_context faliture\n");
        return;
    }
    
    initHwAccel(avcodec_context, video_codec);
    //第五步：打开解码器
    int avcodec_open2_result = avcodec_open2(avcodec_context, video_codec, NULL);
    if (avcodec_open2_result != 0) {
        printf("打开解码器失败");
        return;
    }
}

void AVVideoDecoderFF::decoderVideo() {
    //读取压缩数据AVPacket
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int ret = av_read_frame(avformat_context, pkt);
    if (!ret) {
        if (pkt->stream_index == video_stream_index) {
            decoderPacket(avcodec_context, pkt, frame);
        } else {
//            iOSVideoFrame videoFrame;
//            videoFrame.stream_index = 0;
//            this->callback->onDecodeFrameReceived(callback->bridge, videoFrame);
        }
    } else {
        if (ret == AVERROR_EOF) {
            isEOF = true;
        }
        printf("av_read_frame error");
    }
    av_frame_free(&frame);
    av_packet_free(&pkt);
}

void AVVideoDecoderFF::decoderPacket(AVCodecContext *dec_ctx, AVPacket *pkt, AVFrame *frame) {
    int ret;
    //7. 发送压缩数据给解码器
    ret = avcodec_send_packet(dec_ctx, pkt);
    if(ret == AVERROR(EAGAIN)) {
        fprintf(stderr, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
    } else if (ret < 0) {
        fprintf(stderr, "Error submitting the packet to the decoder, err:%s, pkt_size:%d\n", av_get_err(ret), pkt->size);
        return;
    }

    /*读取所有输出帧(一般可以有任意数目)*/
    while (ret >= 0) {
        //8. 接收解码后的帧, 对于frame, avcodec_receive_frame内部每次都先调用
        //decode 发送进去的是AVPacket, 出来的是AVFrame
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        } else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
        }
        if (frame->format == hw_pix_fmt) {
            auto ptsMs = frame->pts * av_q2d(video_stream->time_base) * 1000;
            auto dtsMs = frame->pkt_dts * av_q2d(video_stream->time_base) * 1000;
            if (frame->key_frame == 1) {
                printf("keyframe pts: %f\n", ptsMs);
            }
            if (frame->pict_type == AV_PICTURE_TYPE_I) {
                printf("I 帧 pts: %f, dts: %f\n", ptsMs, dtsMs);
            } else if (frame->pict_type == AV_PICTURE_TYPE_P) {
                printf("P 帧 pts: %f, dts: %f\n", ptsMs, dtsMs);
            } else if (frame->pict_type == AV_PICTURE_TYPE_B) {
                printf("B 帧 pts: %f, dts: %f\n", ptsMs, dtsMs);
            }
//            iOSVideoFrame videoFrame;
//            videoFrame.plane_data = frame->data[3];
//            videoFrame.width = frame->width;
//            videoFrame.height = frame->height;
//            videoFrame.pts = ptsMs;
//            videoFrame.stream_index = 1;
//            this->callback->onDecodeFrameReceived(callback->bridge, videoFrame);            
        }
    }
}

void AVVideoDecoderFF::destroy() {
    /* 冲刷解码器 draining mode */
    avcodec_send_packet(avcodec_context, NULL);
    avcodec_flush_buffers(avcodec_context);
//    pkt->data = NULL;   // 让其进入drain mode
//    pkt->size = 0;
//    isEOF = true;
//    decoderPacket(avcodec_context, pkt, frame);
    //end
    avcodec_free_context(&avcodec_context);
    avformat_close_input(&avformat_context);
    //用于释放设备缓冲区，同时也会释放其管理的帧缓冲区
    av_buffer_unref(&hw_device_ctx);
}

void AVVideoDecoderFF::decodeFrame() {
    if (isEOF) {
        printf("end of file");
    } else {
        decoderVideo();
    }
}
/*
AVSEEK_FLAG_BACKWARD 1 ///< seek backward seek到timestamp之前的最近关键帧
AVSEEK_FLAG_BYTE 2 ///< seeking based on position in bytes 基于字节位置的跳转
AVSEEK_FLAG_ANY 4 ///< seek to any frame, even non-keyframes 跳转到任意帧，不一定是关键帧
AVSEEK_FLAG_FRAME 8 ///< seeking based on frame number 基于帧数量的跳转
 */
//硬解是否支持seek到任一点
void AVVideoDecoderFF::seekTo(float seekMs) {
    printf("seek to: %f, total duration: %f\n", seekMs, getDurationSEC() * 1000);
    if (seekMs >= getDurationSEC() * 1000) {
        return;
    }
    if (video_stream_index != -1) {
        // 现实时间 -> 时间戳
        AVRational timeBase = avformat_context->streams[video_stream_index]->time_base;
        float seekSeconds_timestamp = seekMs / 1000 * AV_TIME_BASE;
        int64_t video_seek_timestamp = av_rescale_q((int64_t) seekSeconds_timestamp, AV_TIME_BASE_Q, timeBase);
        int ret = av_seek_frame(avformat_context, video_stream_index, video_seek_timestamp, AVSEEK_FLAG_BACKWARD);
        if (ret != 0) {
            printf("seek video frame error : %s", av_err2str(ret));
        }
    }
    else if (audio_stream_index != -1) {
        AVRational time_base = avformat_context->streams[audio_stream_index]->time_base;
        float seekSeconds_timestamp = seekMs / 1000 * AV_TIME_BASE;
        int64_t audio_seek_timestamp = av_rescale_q((int64_t) seekSeconds_timestamp, AV_TIME_BASE_Q, time_base);
        int ret = av_seek_frame(avformat_context, audio_stream_index, audio_seek_timestamp, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
        if (ret != 0) {
            printf("seek video frame error : %s", av_err2str(ret));
        }
    }
    isEOF = false;
}

void AVVideoDecoderFF::destroyHwAccel() {

}
void AVVideoDecoderFF::activateHwAccel() {
    destroy();
    loadResource(filePath);
    avcodec_flush_buffers(avcodec_context);
}
