//
//  decode_filter_audio.cpp
//  MediaLive
//
//  Created by Jason on 2024/4/9.
//

#include "decode_filter_audio.hpp"

/*
说明: 如果某一通道没有输入数据，内部是会去等待数据
 */
#define OUTPUT_AUDIO_SAMPLE_RATE        48000
#define OUTPUT_AUDIO_CHANNELS           2    // 两通道
#define OUTPUT_AUDIO_CHANNEL_LAYOUT     3  // 立体声
const char* filter_descr ="[in0][in1]amix=inputs=2:weights='1 0.25'[out]";

int DecodeFilterAudio::open_input_file_0(const char* filename) {
    int ret;
    const AVCodec* codec;

    // 打开文件
    if ((ret = avformat_open_input(&fmt_ctx0, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx0, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    av_dump_format(fmt_ctx0, 0, filename, 0);

    /* select the audio stream */
    ret = av_find_best_stream(fmt_ctx0, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Cannot find an audio stream in the input file\n");
        return ret;
    }
    audio_stream_index_0 = ret;
    
    const AVCodec *decoder = avcodec_find_decoder(fmt_ctx0->streams[audio_stream_index_0]->codecpar->codec_id);
    dec_ctx0_audio = avcodec_alloc_context3(decoder);
    ret = avcodec_parameters_to_context(dec_ctx0_audio, fmt_ctx0->streams[audio_stream_index_0]->codecpar);

//    dec_ctx0_audio = fmt_ctx0->streams[audio_stream_index_0]->codecpar;
    av_opt_set_int(dec_ctx0_audio, "refcounted_frames", 1, 0);

    /* init the audio decoder */
    if ((ret = avcodec_open2(dec_ctx0_audio, codec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        return ret;
    }

    return 0;
}

int DecodeFilterAudio::open_input_file_1(const char* filename) {
    int ret;
    const AVCodec* codec;

    if ((ret = avformat_open_input(&fmt_ctx1, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx1, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    av_dump_format(fmt_ctx1, 0, filename, 0);

    /* select the audio stream */
    ret = av_find_best_stream(fmt_ctx1, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Cannot find an audio stream in the input file\n");
        return ret;
    }
    audio_stream_index_1 = ret;
    const AVCodec *decoder = avcodec_find_decoder(fmt_ctx1->streams[audio_stream_index_1]->codecpar->codec_id);
    dec_ctx1_audio = avcodec_alloc_context3(decoder);
    ret = avcodec_parameters_to_context(dec_ctx1_audio, fmt_ctx1->streams[audio_stream_index_1]->codecpar);

//    dec_ctx1_audio = fmt_ctx1->streams[audio_stream_index_1]->codec;
    av_opt_set_int(dec_ctx1_audio, "refcounted_frames", 1, 0);

    /* init the audio decoder */
    if ((ret = avcodec_open2(dec_ctx1_audio, codec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        return ret;
    }

    return 0;
}

int DecodeFilterAudio::encode_write_frame(AVFrame* filt_frame, unsigned int stream_index) {
    int ret;
    // av_audio_fifo_write 严格而言这里还需要考虑每次frame报保存的样本数
    ret = avcodec_send_frame(enc_ctx_audio, filt_frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "avcodec_send_frame failed, ret:%d\n", ret);
        return -1;
    }

    while (ret >= 0) {
        AVPacket pkt;
        av_init_packet(&pkt);
        ret = avcodec_receive_packet(enc_ctx_audio, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_receive_packet failed, ret:%d\n", ret);
            return -1;
        }

        /* rescale output packet timestamp values from codec to stream timebase */
        av_packet_rescale_ts(&pkt, enc_ctx_audio->time_base,
                             ofmt_ctx->streams[stream_index]->time_base);
        pkt.stream_index = stream_index;

        /* Write the compressed frame to the media file. */
        // log_packet(fmt_ctx, &pkt);

        pkt.pts = pkt_count_audio * ofmt_ctx->streams[stream_index]->codecpar->frame_size;
        pkt.dts = pkt.pts;
        pkt.duration = ofmt_ctx->streams[stream_index]->codecpar->frame_size;

        pkt.pts = av_rescale_q_rnd(
                    pkt.pts, ofmt_ctx->streams[stream_index]->time_base,
                    ofmt_ctx->streams[stream_index]->time_base,
                    (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = pkt.pts;
        pkt.duration = av_rescale_q_rnd(
                    pkt.duration, ofmt_ctx->streams[stream_index]->time_base,
                    ofmt_ctx->streams[stream_index]->time_base,
                    (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        ++pkt_count_audio;
        av_packet_unref(&pkt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_interleaved_write_frame failed, ret:%d\n", ret);
            return -1;
        }
    }
    return ret;
}

int DecodeFilterAudio::open_output_file(const char* filename)
{
    AVStream* out_stream;
    const AVCodec* encoder;
    int ret;
    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        out_stream->index = 0;
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }

        encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
        enc_ctx_audio = avcodec_alloc_context3(encoder);
        if (!enc_ctx_audio) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_alloc_context3 failed\n");
            return -1;
        }
        const AVCodec *decoder = avcodec_find_decoder(out_stream->codecpar->codec_id);
        enc_ctx_audio = avcodec_alloc_context3(decoder);
        ret = avcodec_parameters_to_context(enc_ctx_audio, out_stream->codecpar);
//        enc_ctx_audio = out_stream->codec;
        enc_ctx_audio->codec_type = AVMEDIA_TYPE_AUDIO;
        enc_ctx_audio->sample_rate = OUTPUT_AUDIO_SAMPLE_RATE;
        enc_ctx_audio->channels = OUTPUT_AUDIO_CHANNELS;
        enc_ctx_audio->channel_layout = OUTPUT_AUDIO_CHANNEL_LAYOUT;
        enc_ctx_audio->sample_fmt = AV_SAMPLE_FMT_FLTP;
        // enc_ctx_audio->bit_rate = 128000;
        enc_ctx_audio->codec_tag = 0;
        enc_ctx_audio->time_base = {1, enc_ctx_audio->sample_rate};

        ret = avcodec_open2(enc_ctx_audio, encoder, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot open audio encoder for stream \n");
            return ret;
        }

        ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx_audio);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "avcodec_parameters_from_context faield, ret:%d\n", ret);
            return ret;
        }

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx_audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_dump_format(ofmt_ctx, 0, filename, 1);

    ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
        return ret;
    }

    /* init Mixer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    return 0;
}

int DecodeFilterAudio::init_audio_filters(const char* filters_descr) {
    char args0[512];
    char args1[512];
    int ret = 0;
    //    int flags = 0;
    const AVFilter* abuffersrc0 = avfilter_get_by_name("abuffer");
    const AVFilter* abuffersrc1 = avfilter_get_by_name("abuffer");
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");

    AVFilterInOut* outputs0 = avfilter_inout_alloc();
    AVFilterInOut* outputs1 = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();

    // 设置混音后输出的音频格式 AV_SAMPLE_FMT_FLTP 立体声 48k
    const enum AVSampleFormat out_sample_fmts[] = {
        AV_SAMPLE_FMT_FLTP  ,// dec_ctx0_audio->sample_fmt,
        (enum AVSampleFormat) - 1};  //{AV_SAMPLE_FMT_S16}
    const int64_t out_channel_layouts[] = {OUTPUT_AUDIO_CHANNEL_LAYOUT, -1};
    const int out_sample_rates[] = {OUTPUT_AUDIO_SAMPLE_RATE, -1};
    // filter链路
    const AVFilterLink* outlink;

    AVRational time_base_0 = fmt_ctx0->streams[audio_stream_index_0]->time_base; // 第一路流
    AVRational time_base_1 = fmt_ctx1->streams[audio_stream_index_1]->time_base; // 第二路流

    filter_graph_audio = avfilter_graph_alloc();  // 创建filter graph
    if (!outputs0 || !inputs || !filter_graph_audio) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    // 第一路流
    /* buffer audio source: the decoded frames from the decoder will be inserted
   * here. */
    if (!dec_ctx0_audio->channel_layout)
        dec_ctx0_audio->channel_layout =
                av_get_default_channel_layout(dec_ctx0_audio->channels);
    snprintf(
                args0, sizeof(args0),
                "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%ld" ,
                time_base_0.num, time_base_0.den, dec_ctx0_audio->sample_rate,
                av_get_sample_fmt_name(dec_ctx0_audio->sample_fmt), dec_ctx0_audio->channel_layout);
    // 把buffersrc_ctx0_audio加入到filter_graph_audio，注意对应的参数
    ret = avfilter_graph_create_filter(&buffersrc_ctx0_audio, abuffersrc0, "in0", args0,
                                       NULL, filter_graph_audio);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }
     // 第二路流
    /* buffer audio source: the decoded frames from the decoder will be inserted here. */
    if (!dec_ctx1_audio->channel_layout)
        dec_ctx1_audio->channel_layout =
                av_get_default_channel_layout(dec_ctx1_audio->channels);
    
    snprintf(
                args1, sizeof(args1),
                "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%ld",
                time_base_1.num, time_base_1.den, dec_ctx1_audio->sample_rate,
                av_get_sample_fmt_name(dec_ctx1_audio->sample_fmt), dec_ctx1_audio->channel_layout);
    // 把buffersrc_ctx1_audio加入到filter_graph_audio，注意对应的参数
    ret = avfilter_graph_create_filter(&buffersrc_ctx1_audio, abuffersrc1, "in1", args1,
                                       NULL, filter_graph_audio);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }
    //audio filter
    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx_audio, abuffersink, "out", NULL,  // 设置输出格式
                                       NULL, filter_graph_audio);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx_audio, "sample_fmts", out_sample_fmts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx_audio, "channel_layouts",
                              out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx_audio, "sample_rates", out_sample_rates,
                              -1, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
        goto end;
    }

    /*
   * Set the endpoints for the filter graph. The filter_graph_audio will
   * be linked to the graph described by filters_descr.
   */

    /*
   * The buffer source output must be connected to the input pad of
   * the first filter described by filters_descr; since the first
   * filter input label is not specified, it is set to "in" by
   * default.
   */
    /*
    outputs是指buffersrc_ctx滤镜的输出引脚(output pad)
    */
    outputs0->name = av_strdup("in0");      // in0开始
    outputs0->filter_ctx = buffersrc_ctx0_audio;
    outputs0->pad_idx = 0;
    outputs0->next = outputs1; // 指向下一个输出

    outputs1->name = av_strdup("in1");
    outputs1->filter_ctx = buffersrc_ctx1_audio;
    outputs1->pad_idx = 0;  // 目前只有一个，并且从0开始
    outputs1->next = NULL;

    /*
   * The buffer sink input must be connected to the output pad of
   * the last filter described by filters_descr; since the last
   * filter output label is not specified, it is set to "out" by
   * default.
   */
    // inputs是指buffersink_ctx滤镜的输入引脚(input pad)
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx_audio;
    inputs->pad_idx = 0;
    inputs->next = NULL;



    if ((ret = avfilter_graph_parse_ptr(filter_graph_audio, filters_descr, &inputs,
                                        &outputs0, NULL)) < 0)  // filter_outputs
    {
        av_log(NULL, AV_LOG_ERROR, "parse ptr fail, ret: %d\n", ret);
        goto end;
    }

    if ((ret = avfilter_graph_config(filter_graph_audio, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "config graph fail, ret: %d\n", ret);
        goto end;
    }

    /* Print summary of the sink buffer
   * Note: args buffer is reused to store channel layout string */
    outlink = buffersink_ctx_audio->inputs[0];
    av_get_channel_layout_string(args0, sizeof(args0), -1,
                                 outlink->channel_layout);
    av_log(NULL, AV_LOG_INFO, "Output: srate:%dHz fmt:%s chlayout:%s\n",
           (int)outlink->sample_rate,
           (char*)av_x_if_null(
               av_get_sample_fmt_name((enum AVSampleFormat)outlink->format), "?"),
           args0);

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs0);

    return ret;
}


int DecodeFilterAudio::main11(const char *in0, const char *in1, const char *out)
{
    int ret;
    int got_frame;
    int input0_eof = 0;  // 文件是否已经读完毕
    int input1_eof = 0;

    int input0_frames = 0;
    int input1_frames = 0;
    int output_frames = 0;
    int count = 0;
    AVFrame* frame = av_frame_alloc();
    AVFrame* filt_frame = av_frame_alloc();

    if (!frame || !filt_frame) {
        perror("Could not allocate frame");
        return 1;
    }


//    const char *in0 = "/Users/jason/Jason/project/MediaLive/MediaLive/MediaLive/doc/48khz-0.aac";
//    const char *in1 = "/Users/jason/Jason/project/MediaLive/MediaLive/MediaLive/doc/48khz-1.aac";
//    const char *out = "/Users/jason/Jason/project/MediaLive/MediaLive/MediaLive/doc/out.aac";
    //打开第一个输入文件
    if ((ret = open_input_file_0(in0) < 0)) {
        av_log(NULL, AV_LOG_ERROR, "open input file fail, ret: %d\n", ret);
        goto end;
    }
    //打开第二个输入文件
    if ((ret = open_input_file_1(in1)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "open input file fail, ret: %d\n", ret);
        goto end;
    }
     //初始化filter
    if ((ret = init_audio_filters(filter_descr)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "init filters fail, ret: %d\n", ret);
        goto end;
    }
    //打开输出文件
    if ((ret = open_output_file(out)) < 0)  {
       goto end;
    }

    // 初始化packet
    // 第一路流
    AVPacket packet0;
    memset(&packet0, 0, sizeof(AVPacket));
    AVPacket _packet0;
    memset(&_packet0, 0, sizeof(AVPacket));

     // 第二路流
    AVPacket packet1;
    memset(&packet1, 0, sizeof(AVPacket));
    AVPacket _packet1;
    memset(&_packet1, 0, sizeof(AVPacket));

    while (1) {
        if(input0_eof !=1)  {
            if (!_packet0.data) {
                if ((ret = av_read_frame(fmt_ctx0, &packet0)) < 0)
                {
                    input0_eof = 1;
                    printf("file0 finish\n");
                    // 通知filter, in0数据结束了
                    av_buffersrc_add_frame_flags(buffersrc_ctx0_audio, NULL, AV_BUFFERSRC_FLAG_PUSH);
                    continue;
                }
                _packet0 = packet0;     // 暂存
            }

            if (packet0.stream_index == audio_stream_index_0) {
                got_frame = 0;
                avcodec_send_packet(dec_ctx0_audio, &packet0);
                ret = avcodec_receive_frame(dec_ctx0_audio, frame);
//                ret = avcodec_decode_audio4(dec_ctx0_audio, frame, &got_frame, &packet0);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error decoding audio\n");
                    continue;
                }
                packet0.size -= ret;        // 检测是否还有数据
                packet0.data += ret;
                got_frame = 1;
                if (got_frame) {
                    /* push the audio data from decoded frame into the filtergraph */
                    input0_frames++;
                    if (av_buffersrc_add_frame_flags(buffersrc_ctx0_audio, frame, AV_BUFFERSRC_FLAG_PUSH) < 0) {
                        av_log(NULL, AV_LOG_ERROR,
                               "Error while feeding the audio filtergraph\n");
                        break;
                    }
                }

                if (packet0.size <= 0)
                    av_packet_unref(&_packet0);  // 释放资源
            } else {
                /* discard non-wanted packets */
                av_packet_unref(&_packet0);
            }
        }

        if(input1_eof !=1  && count++ % 1 == 0) {
            if (!_packet1.data) {
                if ((ret = av_read_frame(fmt_ctx1, &packet1)) < 0){
                    input1_eof = 1;
//                    av_buffersrc_add_frame_flags(buffersrc_ctx1_audio, NULL, AV_BUFFERSRC_FLAG_PUSH);
                    printf("file1 finish\n");
                    continue;
                }
                _packet1 = packet1;
            }

            if (packet1.stream_index == audio_stream_index_1) {
                got_frame = 0;
                avcodec_send_packet(dec_ctx0_audio, &packet0);
                ret = avcodec_receive_frame(dec_ctx0_audio, frame);

//                ret = avcodec_decode_audio4(dec_ctx1_audio, frame, &got_frame, &packet1);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error decoding audio\n");
                    continue;
                }
                got_frame = 1;
                packet1.size -= ret;
                packet1.data += ret;

                if (got_frame) {
                    input1_frames++;
                    /* push the audio data from decoded frame into the filtergraph */
                    if (av_buffersrc_add_frame_flags(buffersrc_ctx1_audio, frame, AV_BUFFERSRC_FLAG_PUSH) < 0) {
                        av_log(NULL, AV_LOG_ERROR,
                               "Error while feeding the audio filtergraph\n");
                        break;
                    }
                }

                if (packet1.size <= 0)
                    av_packet_unref(&_packet1);
            } else {
                /* discard non-wanted packets */
                av_packet_unref(&_packet1);
            }
        }
        /* pull filtered audio from the filtergraph */
        if (got_frame) {
            while (1) {
                ret = av_buffersink_get_frame(buffersink_ctx_audio, filt_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                    printf("get  EAGAIN or AVERROR_EOF\n");
                    break;
                }
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "buffersink get frame fail, ret: %d\n",
                           ret);
                    break;
                }
                output_frames++;
                encode_write_frame(filt_frame, 0);
                av_frame_unref(filt_frame);
            }
        }
        if(input0_frames %100 == 0 || input1_frames %100 == 0  || output_frames %100 == 0) {
            printf("in0: %d, in1: %d, out: %d\n", input0_frames, input1_frames, output_frames);
        }

        if(input0_eof == 1 && input1_eof==1) {
            printf("two file finish\n");
            break;
        }
    }

end:
    avfilter_graph_free(&filter_graph_audio);
    if(dec_ctx0_audio)
        avcodec_close(dec_ctx0_audio);
    avformat_close_input(&fmt_ctx0);
    avcodec_close(dec_ctx1_audio);
    avformat_close_input(&fmt_ctx1);
    av_frame_free(&frame);
    av_frame_free(&filt_frame);

    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred: %d\n",ret);
        exit(1);
    }

    av_write_trailer(ofmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);


    return 0;
}

