//
//  ReceiverViewController.m
//  FFmpegiOS
//
//  Created by Jason on 2020/12/7.
//  Copyright © 2020 Jason. All rights reserved.
//

#import "ReceiverViewController.h"
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
/**
 * 最简单的基于FFmpeg的收流器（接收RTMP）
 * 本例子将流媒体数据（以RTMP为例）保存成本地文件。
 * 是使用FFmpeg进行流媒体接收最简单的教程。
 */
//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 1

@interface ReceiverViewController ()
@property(nonatomic, strong)NSString* outFilePath;
@end

@implementation ReceiverViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.whiteColor;
}

- (NSString *)creatFile:(NSString *)fileName {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *path = [paths objectAtIndex:0];
    NSString *tmpPath = [path stringByAppendingPathComponent:@"temp"];
    [[NSFileManager defaultManager] createDirectoryAtPath:tmpPath withIntermediateDirectories:YES attributes:nil error:NULL];
    NSString* outFilePath = [tmpPath stringByAppendingPathComponent:[NSString stringWithFormat:@"%@", fileName]];
    return outFilePath;
}
- (void)receiveAction {
    _outFilePath = [self creatFile:@"receive.flv"];
    AVOutputFormat *ofmt = NULL;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int videoindex=-1;
    int frame_index=0;
    in_filename  = "rtmp://192.168.0.8/live/livestream";
    //in_filename  = "rtp://233.233.233.233:6666";
    //out_filename = "receive.ts";
    //out_filename = "receive.mkv";
//    out_filename = "receive.flv";
    out_filename = _outFilePath.UTF8String;
//    av_register_all();
    //Network
    avformat_network_init();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf( "Could not open input file.");
        goto end;
    }
    //通过AVDictionary来改变AVFormatContext结构体里参数
     AVDictionary* avdic = NULL;
     av_dict_set(&avdic, "probesize", "2048", 0);
     av_dict_set(&avdic, "max_analyze_duration", "10", 0);
    if ((ret = avformat_find_stream_info(ifmt_ctx, &avdic)) < 0) {
        printf( "Failed to retrieve input stream information");
        goto end;
    }

    for(i=0; i<ifmt_ctx->nb_streams; i++)
        if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            videoindex=i;
            break;
        }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    //Output
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename); //RTMP

    if (!ofmt_ctx) {
        printf( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        if (!out_stream) {
            printf( "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        AVCodecContext *in_codecCtx = in_stream->codec;
        //Copy the settings of AVCodecContext
        ret = avcodec_copy_context(out_stream->codec, in_codecCtx);
        if (ret < 0) {
            printf( "Failed to copy context from input to output stream codec context\n");
            goto end;
        }
        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    //Dump Format------------------
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    //Open output URL
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf( "Could not open output URL '%s'", out_filename);
            goto end;
        }
    }
    //Write file header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        printf( "Error occurred when opening output URL\n");
        goto end;
    }

#if USE_H264BSF
    AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb");
#endif

    while (1) {
        AVStream *in_stream, *out_stream;
        //Get an AVPacket
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;
        
        in_stream  = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        /* copy packet */
        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        //Print to Screen
        if(pkt.stream_index==videoindex){
            printf("Receive %8d video frames from input URL\n",frame_index);
            frame_index++;

#if USE_H264BSF
            av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
        }
        //ret = av_write_frame(ofmt_ctx, &pkt);
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);

        if (ret < 0) {
            printf( "Error muxing packet\n");
            break;
        }
        
//        av_free_packet(&pkt);
        av_packet_unref(&pkt);
    }

#if USE_H264BSF
    av_bitstream_filter_close(h264bsfc);
#endif

    AVDictionary * opts = NULL;
    av_dict_set(&opts, "flvflags", "no_duration_filesize", 0);
    int e = avformat_write_header(ofmt_ctx, opts ? &opts : NULL);
    //Write file trailer
    av_write_trailer(ofmt_ctx);
end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf( "Error occurred.\n");
//        return -1;
    }
//    return 0;
}

// ffmpeg -re -i tnhaoxc.flv -c copy -f flv rtmp://192.168.0.104/live
// ffmpeg -i rtmp://192.168.0.104/live -c copy tnlinyrx.flv
// ./streamer tnhaoxc.flv rtmp://192.168.0.104/live
// ./streamer rtmp://192.168.0.104/live tnhaoxc.flv
- (int) receive {
    _outFilePath = [self creatFile:@"receive.flv"];
    int frame_index=0;

    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    const char *in_filename, *out_filename;
    int ret, i;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
//
//    if (argc < 3) {
//        printf("usage: %s input output\n"
//               "API example program to remux a media file with libavformat and libavcodec.\n"
//               "The output format is guessed according to the file extension.\n"
//               "\n", argv[0]);
//        return 1;
//    }

    in_filename  = "rtmp://192.168.0.8/live/livestream";
    out_filename = _outFilePath.UTF8String;

    // 1. 打开输入
    // 1.1 读取文件头，获取封装格式相关信息
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf("Could not open input file '%s'", in_filename);
        goto end;
    }
    
    // 1.2 解码一段数据，获取流相关信息
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    // 2. 打开输出
    // 2.1 分配输出ctx
    bool push_stream = false;
    char *ofmt_name = NULL;
    if (strstr(out_filename, "rtmp://") != NULL) {
        push_stream = true;
        ofmt_name = "flv";
    }
    else if (strstr(out_filename, "udp://") != NULL) {
        push_stream = true;
        ofmt_name = "mpegts";
    }
    else {
        push_stream = false;
        ofmt_name = NULL;
    }
    avformat_alloc_output_context2(&ofmt_ctx, NULL, ofmt_name, out_filename);
    if (!ofmt_ctx) {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    stream_mapping_size = ifmt_ctx->nb_streams;
    stream_mapping = av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ofmt = ofmt_ctx->oformat;

    AVRational frame_rate;
    double duration = 0.0;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;

        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }

        if (push_stream && (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO)) {
            frame_rate = av_guess_frame_rate(ifmt_ctx, in_stream, NULL);
            duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
        }

        stream_mapping[i] = stream_index++;

        // 2.2 将一个新流(out_stream)添加到输出文件(ofmt_ctx)
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            printf("Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        // 2.3 将当前输入流中的参数拷贝到输出流中
        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            printf("Failed to copy codec parameters\n");
            goto end;
        }
        out_stream->codecpar->codec_tag = 0;
    }
    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    if (!(ofmt->flags & AVFMT_NOFILE)) {    // TODO: 研究AVFMT_NOFILE标志
        // 2.4 创建并初始化一个AVIOContext，用以访问URL(out_filename)指定的资源
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf("Could not open output file '%s'", out_filename);
            goto end;
        }
    }

    // 3. 数据处理
    // 3.1 写输出文件头
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        printf("Error occurred when opening output file\n");
        goto end;
    }

    while (1) {
        AVStream *in_stream, *out_stream;

        // 3.2 从输出流读取一个packet
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
            break;
        }

        in_stream  = ifmt_ctx->streams[pkt.stream_index];
        if (pkt.stream_index >= stream_mapping_size ||
            stream_mapping[pkt.stream_index] < 0) {
            av_packet_unref(&pkt);
            continue;
        }

        int codec_type = in_stream->codecpar->codec_type;
        if (push_stream && (codec_type == AVMEDIA_TYPE_VIDEO)) {
            av_usleep((int)(duration*AV_TIME_BASE));
        }

        pkt.stream_index = stream_mapping[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];

        /* copy packet */
        // 3.3 更新packet中的pts和dts
        // 关于AVStream.time_base(容器中的time_base)的说明：
        // 输入：输入流中含有time_base，在avformat_find_stream_info()中可取到每个流中的time_base
        // 输出：avformat_write_header()会根据输出的封装格式确定每个流的time_base并写入文件中
        // AVPacket.pts和AVPacket.dts的单位是AVStream.time_base，不同的封装格式AVStream.time_base不同
        // 所以输出文件中，每个packet需要根据输出封装格式重新计算pts和dts
        av_packet_rescale_ts(&pkt, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        printf("Receive %8d video frames from input URL\n",frame_index);
        frame_index++;
        // 3.4 将packet写入输出
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            printf("Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);
    }
    printf("Receive Finished");

    // 3.5 写输出文件尾
    av_write_trailer(ofmt_ctx);

end:
    avformat_close_input(&ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);

    av_freep(&stream_mapping);

    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred: %s\n", av_err2str(ret));
        return 1;
    }

    return 0;
}

@end
