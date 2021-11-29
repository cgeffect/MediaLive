//
//  AVDemuxer.cpp
//  FFmpegiOS
//
//  Created by Jason on 2020/11/11.
//  Copyright © 2020 Jason. All rights reserved.
//

#include "AVVideoDemuxer.h"
#include <stdio.h>
#include <libavformat/avformat.h>
#include <string>

int AVVideoDemuxer::ffmpeg_demux_(const char *path) {
    //打开网络流。这里如果只需要读取本地媒体文件，不需要用到网络功能，可以不用加上这一句
//    avformat_network_init();

    const char *in_filename = path;

    printf("in_filename = %s\n", in_filename);

    int videoindex = -1;        // 视频索引


    // 打开文件，主要是探测协议类型，如果是网络文件则创建网络链接
    //avformat_open_input, AVFormatContext参数可以先初始化传进来, 也可以不初始化传进来,
    //未初始化传进来, avformat_open_input会先进行初始化操作
    //AVFormatContext是描述一个媒体文件或媒体流的构成和基本信息的结构体
    //1. 分配解复用器内存
    AVFormatContext *ifmt_ctx = avformat_alloc_context();           // 输入文件的demux
    AVDictionary     *opts          = NULL;
    av_dict_set(&opts, "timeout", "1000000", 0);//设置超时1秒
    
    //2. 根据url打开码流，并选择匹配的解复用器
    int ret = avformat_open_input(&ifmt_ctx, in_filename, NULL, NULL);
    if (ret < 0)  //如果打开媒体文件失败，打印失败原因
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        printf("open %s failed:%s\n", in_filename, buf);
        demux_failed(ifmt_ctx);
        return 0;
    }
    av_dict_free(&opts);

    //3. 读取媒体文件的部分数据包以获取码流信息
    ret = avformat_find_stream_info(ifmt_ctx, NULL);
    if (ret < 0)  //如果打开媒体文件失败，打印失败原因
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        printf("avformat_find_stream_info %s failed:%s\n", in_filename, buf);
        demux_failed(ifmt_ctx);
        return 0;
    }

    /*
     * 老版本通过遍历的方式读取媒体文件视频和音频的信息
     * 新版本的FFmpeg新增加了函数av_find_best_stream，也可以取得同样的效果
     */
    for (uint32_t i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream *in_stream = ifmt_ctx->streams[i];// 音频流、视频流、字幕流
        AVCodecParameters *codeContext = in_stream->codecpar;
        if (AVMEDIA_TYPE_VIDEO == codeContext->codec_type)  //如果是视频流，则打印视频的信息
        {
            videoindex = i;
        }
    }
    
    videoStreamIndex = videoindex;
    fmt_ctx = ifmt_ctx;
    return 0;
    //读取数据, 获取视频信息
    //获取视频流
}

void AVVideoDemuxer::demux_failed(AVFormatContext *ifmt_ctx) {
    if (ifmt_ctx) {
        avformat_close_input(&ifmt_ctx);
    }
}
