//
//  video_filter1.cpp
//  MediaLive
//
//  Created by Jason on 2024/4/9.
//

#include "video_filter_decode.hpp"
/**
 * 最简单的基于FFmpeg的AVFilter例子（叠加水印）
 * Simplest FFmpeg AVfilter Example (Watermark)
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * 本程序使用FFmpeg的AVfilter实现了视频的水印叠加功能。
 * 可以将一张PNG图片作为水印叠加到视频上。
 * 是最简单的FFmpeg的AVFilter方面的教程。
 * 适合FFmpeg的初学者。
 *
 * This software uses FFmpeg's AVFilter to add watermark in a video file.
 * It can add a PNG format picture as watermark to a video file.
 * It's the simplest example based on FFmpeg's AVFilter.
 * Suitable for beginner of FFmpeg
 *
 */
#include <stdio.h>
#include <string>
 
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
};
#endif

 
static AVFormatContext *pFormatCtx;
static AVCodecContext *pCodecCtx;
AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;
static int video_stream_index = -1;
 
 
static void avErrorMsg(int errnumn, std::string &buffer) {
    buffer.assign(std::max(AV_ERROR_MAX_STRING_SIZE, 128) + 1, '\0');
    av_make_error_string(buffer.data(), buffer.size(), errnumn);
    size_t len = strnlen(buffer.data(), buffer.size());
    buffer.resize(len);
}

static int open_input_file(const char *filename)
{
    int ret;
    const AVCodec *dec;
 
    if ((ret = avformat_open_input(&pFormatCtx, filename, NULL, NULL)) < 0) {
        printf( "Cannot open input file\n");
        return ret;
    }
 
    if ((ret = avformat_find_stream_info(pFormatCtx, NULL)) < 0) {
        printf( "Cannot find stream information\n");
        return ret;
    }
 
    /* select the video stream */
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        printf( "Cannot find a video stream in the input file\n");
        return ret;
    }
    video_stream_index = ret;
    
    /* create decoding context */
    pCodecCtx = avcodec_alloc_context3(dec);
    if (!pCodecCtx)
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[video_stream_index]->codecpar);
    
    if (ret < 0) {
        printf( "avcodec_parameters_to_context fail\n");
        return 0;
    }
//    pCodecCtx = pFormatCtx->streams[video_stream_index]->codec;
 
    /* init the video decoder */
    if ((ret = avcodec_open2(pCodecCtx, dec, NULL)) < 0) {
        printf( "Cannot open video decoder\n");
        return ret;
    }
 
    return 0;
}
 
static int init_filters(const char *filters_descr)
{
    char args[512];
    bzero(args, 512);
    int ret;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = pFormatCtx->streams[video_stream_index]->time_base;
 
    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        return  0;
    }
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            pCodecCtx->width, pCodecCtx->height, 
             pCodecCtx->pix_fmt,
            time_base.num, time_base.den,
            pCodecCtx->sample_aspect_ratio.num, pCodecCtx->sample_aspect_ratio.den);
 
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    std::string error = "";
    avErrorMsg(ret, error);
    if (ret < 0) {
        printf("Cannot create buffer source\n");
        return ret;
    }
 
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);

    if (ret < 0) {
        printf("Cannot create buffer sink\n");
        return ret;
    }
 
    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
 
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
 
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                    &inputs, &outputs, NULL)) < 0)
        return ret;
 
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        return ret;
    return 0;
}
 
 
int video_filter_decode(const char *in, const char *in1, const char *output)
{
    int ret;
    AVPacket packet;
    AVFrame *pFrame;
    AVFrame *pFrame_out;
 
    int got_frame;
  
    if ((ret = open_input_file(in)) < 0) {
        printf("open_input_file\n");

        return 0;
//        goto end;
    }
    std::string logo = in1;
    std::string desc = "movie=" + logo + "[wm];[in][wm]overlay=5:5[out]";
    //"movie=my_logo.png[wm];[in][wm]overlay=5:5[out]";
    const char *filter_descr = desc.c_str();
    if ((ret = init_filters(filter_descr)) < 0) {
        printf("init_filters\n");
        return 0;
//        goto end;
    }
 
    FILE *fp_yuv=fopen(output,"wb+");
    pFrame=av_frame_alloc();
    pFrame_out=av_frame_alloc();
 
    /* read all packets */
    while (1) {
 
        ret = av_read_frame(pFormatCtx, &packet);
        if (ret< 0)
            break;
 
        if (packet.stream_index == video_stream_index) {
            got_frame = 1;
            ret = avcodec_send_packet(pCodecCtx, &packet);
            
            ret = avcodec_receive_frame(pCodecCtx, pFrame);
//            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_frame, &packet);
            if (ret == -35) {
                continue;
            }
            if (ret < 0) {
                printf( "Error decoding video\n");
                break;
            }
 
            if (got_frame) {
//                pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
                
                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame(buffersrc_ctx, pFrame) < 0) {
                    printf( "Error while feeding the filtergraph\n");
                    break;
                }
 
                /* pull filtered pictures from the filtergraph */
                while (1) {
 
                    ret = av_buffersink_get_frame(buffersink_ctx, pFrame_out);
                    if (ret < 0)
                        break;
 
                    printf("Process 1 frame!\n");
 
                    if (pFrame_out->format==AV_PIX_FMT_YUV420P) {
                        //Y, U, V
                        for(int i=0;i<pFrame_out->height;i++){
                            fwrite(pFrame_out->data[0]+pFrame_out->linesize[0]*i,1,pFrame_out->width,fp_yuv);
                        }
                        for(int i=0;i<pFrame_out->height/2;i++){
                            fwrite(pFrame_out->data[1]+pFrame_out->linesize[1]*i,1,pFrame_out->width/2,fp_yuv);
                        }
                        for(int i=0;i<pFrame_out->height/2;i++){
                            fwrite(pFrame_out->data[2]+pFrame_out->linesize[2]*i,1,pFrame_out->width/2,fp_yuv);
                        }
                    }
                    av_frame_unref(pFrame_out);
                }
            }
            av_frame_unref(pFrame);
        }
        av_packet_unref(&packet);
    }
    fclose(fp_yuv);
 
end:
    avfilter_graph_free(&filter_graph);
    if (pCodecCtx)
        avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
 
 
    if (ret < 0 && ret != AVERROR_EOF) {
        char buf[1024];
        av_strerror(ret, buf, sizeof(buf));
        printf("Error occurred: %s\n", buf);
        return -1;
    }
 
    return 0;
}
 
