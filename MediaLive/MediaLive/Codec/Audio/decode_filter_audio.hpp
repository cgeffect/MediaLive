//
//  decode_filter_audio.hpp
//  MediaLive
//
//  Created by Jason on 2024/4/9.
//

#ifndef decode_filter_audio_hpp
#define decode_filter_audio_hpp

#include <stdio.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

#include <string>
#include <iostream>

class DecodeFilterAudio {
    
    
public:
    
    AVFormatContext* fmt_ctx0 = avformat_alloc_context();  //第一路流
    AVFormatContext* fmt_ctx1 = avformat_alloc_context();  //第二路流

    AVCodecContext* dec_ctx0_audio;
    AVCodecContext* dec_ctx1_audio;

    AVFilterContext* buffersink_ctx_audio;
    AVFilterContext* buffersrc_ctx0_audio; //in0
    AVFilterContext* buffersrc_ctx1_audio; //in1

    AVFormatContext* ofmt_ctx;      // 输出文件
    AVCodecContext*  enc_ctx_audio;  //输出文件的音频的codec
    int pkt_count_audio = 0;


     AVFilterGraph* filter_graph_audio; // filter链路
    int audio_stream_index_0 = -1;  // 第一路流
    int audio_stream_index_1 = -1;  // 第二路流

public:
    int open_input_file_0(const char* filename);
    int open_input_file_1(const char* filename);
    int encode_write_frame(AVFrame* filt_frame, unsigned int stream_index);
    int open_output_file(const char* filename);
    int init_audio_filters(const char* filters_descr);
    int main11(const char *in0, const char *in1, const char *out);
};
#endif /* decode_filter_audio_hpp */
