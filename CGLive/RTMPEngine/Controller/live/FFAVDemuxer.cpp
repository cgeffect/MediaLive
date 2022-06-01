//
//  FFAVDemuxer.cpp
//  Mogic
//
//  Created by Jason on 2022/5/31.
//  Copyright © 2022 Jason. All rights reserved.
//

#include "FFAVDemuxer.h"

namespace mogic {

FFAVDemuxer::FFAVDemuxer() {
    
}

FFAVDemuxer::~FFAVDemuxer() {
    
}

int FFAVDemuxer::loadResource(const char *inPath) {
    avformat_network_init();

    // 打开文件，主要是探测协议类型，如果是网络文件则创建网络链接
    //avformat_open_input, AVFormatContext参数可以先初始化传进来, 也可以不初始化传进来,
    //未初始化传进来, avformat_open_input会先进行初始化操作
    //AVFormatContext是描述一个媒体文件或媒体流的构成和基本信息的结构体
    
    //2. 根据url打开码流，并选择匹配的解复用器
    int ret = avformat_open_input(&ifmt_ctx, inPath, NULL, NULL);
    if (ret < 0) {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        printf("open %s failed:%s\n", inPath, buf);
        stopDemux();
        return -1;
    }

    //3. 读取媒体文件的部分数据包以获取码流信息
    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if (ret < 0) {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        printf("avformat_find_stream_info %s failed:%s\n", inPath, buf);
        stopDemux();
        return -1;
    }

    //打开媒体文件成功
    printf("\n==== av_dump_format in_filename:%s ===\n", inPath);
    av_dump_format(ifmt_ctx, 0, inPath, 0);
    printf("\n==== av_dump_format finish =======\n\n");
    // url: 调用avformat_open_input读取到的媒体文件的路径/名字
    printf("media name:%s\n", ifmt_ctx->url);
    // nb_streams: nb_streams媒体流数量
    printf("stream number:%d\n", ifmt_ctx->nb_streams);
    // bit_rate: 媒体文件的码率,单位为bps
    printf("media average ratio:%lldkbps\n",(int64_t)(ifmt_ctx->bit_rate/1024));
    // 时间
    long long total_seconds, hour, minute, second;
    // duration: 媒体文件时长，单位微妙
    total_seconds = (ifmt_ctx->duration) / AV_TIME_BASE;  // 1000us = 1ms, 1000ms = 1秒
    hour = total_seconds / 3600;
    minute = (total_seconds % 3600) / 60;
    second = (total_seconds % 60);
    //通过上述运算，可以得到媒体文件的总时长
    printf("total duration: %02lld:%02lld:%02lld\n", hour, minute, second);
    printf("\n");
    /*
     * 老版本通过遍历的方式读取媒体文件视频和音频的信息
     * 新版本的FFmpeg新增加了函数av_find_best_stream，也可以取得同样的效果
     */
    
    for (uint32_t i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream *in_stream = ifmt_ctx->streams[i];
        if (AVMEDIA_TYPE_AUDIO == in_stream->codecpar->codec_type) {
            printf("----- Audio info:\n");
            // index: 每个流成分在ffmpeg解复用分析后都有唯一的index作为标识
            printf("stream_index:%d\n", in_stream->index);
            // sample_rate: 音频编解码器的采样率，单位为Hz
            printf("samplerate:%dHz\n", in_stream->codecpar->sample_rate);
            // codecpar->format: 音频采样格式
            if (AV_SAMPLE_FMT_FLTP == in_stream->codecpar->format) {
                printf("sampleformat:AV_SAMPLE_FMT_FLTP\n");
            } else if (AV_SAMPLE_FMT_S16P == in_stream->codecpar->format) {
                printf("sampleformat:AV_SAMPLE_FMT_S16P\n");
            }
            // channels: 音频信道数目
            printf("channel number:%d\n", in_stream->codecpar->channels);
            // codec_id: 音频压缩编码格式
            if (AV_CODEC_ID_AAC == in_stream->codecpar->codec_id) {
                printf("audio codec:AAC\n");
            } else if (AV_CODEC_ID_MP3 == in_stream->codecpar->codec_id) {
                printf("audio codec:MP3\n");
            } else {
                printf("audio codec_id:%d\n", in_stream->codecpar->codec_id);
            }
            // 音频总时长，单位为秒。注意如果把单位放大为毫秒或者微妙，音频总时长跟视频总时长不一定相等的
            if(in_stream->duration != AV_NOPTS_VALUE) {
                int duration_audio = (in_stream->duration) * av_q2d(in_stream->time_base);
                //将音频总时长转换为时分秒的格式打印到控制台上
                printf("audio duration: %02d:%02d:%02d\n",
                       duration_audio / 3600, (duration_audio % 3600) / 60, (duration_audio % 60));
            } else {
                printf("audio duration unknown");
            }

            printf("\n");

            audioindex = i; // 获取音频的索引
        } else if (AVMEDIA_TYPE_VIDEO == in_stream->codecpar->codec_type) {
            //如果是视频流，则打印视频的信息
            printf("----- Video info:\n");
            printf("stream_index:%d\n", in_stream->index);
            // avg_frame_rate: 视频帧率,单位为fps，表示每秒出现多少帧
            printf("fps:%lffps\n", av_q2d(in_stream->avg_frame_rate));
            printf("video codec_id:%d\n", in_stream->codecpar->codec_id);

            // 视频帧宽度和帧高度
            printf("width:%d height:%d\n", in_stream->codecpar->width,
                   in_stream->codecpar->height);
            //视频总时长，单位为秒。注意如果把单位放大为毫秒或者微妙，音频总时长跟视频总时长不一定相等的
            if(in_stream->duration != AV_NOPTS_VALUE) {
                int duration_video = (in_stream->duration) * av_q2d(in_stream->time_base);
                printf("video duration: %02d:%02d:%02d\n",
                       duration_video / 3600,
                       (duration_video % 3600) / 60,
                       (duration_video % 60)); //将视频总时长转换为时分秒的格式打印到控制台上
            } else {
                printf("video duration unknown");
            }

            printf("\n");
            videoindex = i;
        }
    }

    pkt = av_packet_alloc();
    return 0;
}

AVPacket *FFAVDemuxer::demuxerPacket() {
    int ret = 0;
    printf("\n-----av_read_frame start\n");
    ret = av_read_frame(ifmt_ctx, pkt);
    if (ret < 0) {
        printf("av_read_frame end\n");
    }

    if (pkt->stream_index == audioindex) {
        printf("audio pts: %lld\n", pkt->pts);
        printf("audio dts: %lld\n", pkt->dts);
        printf("audio size: %d\n", pkt->size);
        printf("audio pos: %lld\n", pkt->pos);
        printf("audio duration: %lf\n\n",
               pkt->duration * av_q2d(ifmt_ctx->streams[audioindex]->time_base));
    } else if (pkt->stream_index == videoindex) {
        printf("video pts: %lld\n", pkt->pts);
        printf("video dts: %lld\n", pkt->dts);
        printf("video size: %d\n", pkt->size);
        printf("video pos: %lld\n", pkt->pos);
        printf("video duration: %lf\n\n",
               pkt->duration * av_q2d(ifmt_ctx->streams[videoindex]->time_base));
    } else {
        printf("unknown stream_index:%d\n", pkt->stream_index);
    }
    pkt_count++;
    return pkt;
}

void FFAVDemuxer::freePacket() {
    if (pkt) {
        av_packet_unref(pkt);
    }
}

void FFAVDemuxer::stopDemux() {
    if(ifmt_ctx) {
        avformat_close_input(&ifmt_ctx);
    }
    if(pkt) {
        av_packet_free(&pkt);
    }
}
}

