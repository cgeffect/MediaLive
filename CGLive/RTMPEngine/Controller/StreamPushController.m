//
//  StreamPushController.m
//  FFmpegiOS
//
//  Created by Jason on 2020/11/30.
//  Copyright © 2020 Jason. All rights reserved.
//

#import "StreamPushController.h"
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#import "StreamerViewController.h"
#import "ReceiverViewController.h"
#import "PullRTMP.h"

@interface StreamPushController ()
{
    PullRTMP *_pull;
}
@property (strong, nonatomic) NSString *output;

@end

@implementation StreamPushController

- (void)viewDidLoad {
    [super viewDidLoad];
    _output = @"rtmp://172.16.184.26:1935/live/livestream";
    _pull = [[PullRTMP alloc] init];
}
//推流
- (IBAction)streamer:(id)sender {
    StreamerViewController *vc = [[StreamerViewController alloc] init];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [vc streamerAction];
    });
}

//拉流
- (IBAction)receiver:(id)sender {
    ReceiverViewController *vc = [[ReceiverViewController alloc] init];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
//        [vc receiveAction];
//        [vc receive];
        [_pull loadPath];
    });
}
- (IBAction)rtmp:(id)sender {
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        PullRTMP *_pull = [[PullRTMP alloc] init];
        [_pull push];
    });

}

//推流
- (IBAction)clickStream:(id)sender {
    NSString *input_nsstr= [[NSBundle mainBundle] pathForResource:@"source.200kbps.768x320" ofType:@"flv"];
    
    AVOutputFormat *ofmt = NULL;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt = {0};
    const char *in_filename, *out_filename;
    int ret, i;
    int videoindex=-1;
    int frame_index=0;
    int audioIndex = -1;
    int pcm_Index = 0;
    int64_t start_time=0;
    //in_filename  = "cuc_ieschool.mov";
    //in_filename  = "cuc_ieschool.h264";
    //in_filename  = "cuc_ieschool.flv";//Input file URL
    //out_filename = "rtmp://localhost/publishlive/livestream";//Output URL[RTMP]
    //out_filename = "rtp://233.233.233.233:6666";//Output URL[UDP]
    in_filename = input_nsstr.UTF8String;
    out_filename = "rtmp://172.16.184.26:1935/live/livestream";

    //    av_register_all();
    //Network
    avformat_network_init();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf( "Could not open input file.");
        goto end;
    }
    printf( "in_filename: %s\n", in_filename);
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf( "Failed to retrieve input stream information");
        goto end;
    }
    printf( "out_filename: %s\n", out_filename);
    
    for(i=0; i<ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodecParameters *param = stream->codecpar;
        if(param->codec_type==AVMEDIA_TYPE_VIDEO){
            videoindex=i;
            break;
        }
    }
    for(i=0; i<ifmt_ctx->nb_streams; i++) {
        if(ifmt_ctx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            audioIndex=i;
            break;
        }
    }
    
    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    //Output
    
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename); //RTMP
    //avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//UDP
    
    if (!ofmt_ctx) {
        printf( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    ofmt = ofmt_ctx->oformat;
    
    printf("name:%s, long_name:%s, mime_type:%s, extensions:%s\n",
           ofmt->name, ofmt->long_name, ofmt->mime_type, ofmt->extensions);
    
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        
        AVStream *in_stream = ifmt_ctx->streams[i];
        if(in_stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO && in_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        AVCodec *in_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            printf( "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        AVCodecContext *codec_ctx = avcodec_alloc_context3(in_codec);
        ret = avcodec_parameters_to_context(codec_ctx, in_stream->codecpar);
        if (ret < 0){
            printf("Failed to copy in_stream codecpar to codec context\n");
            goto end;
        }
         
        codec_ctx->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
         
        ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
        if (ret < 0){
            printf("Failed to copy codec context to out_stream codecpar context\n");
            goto end;

        }
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

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        printf( "Error occurred when opening output URL\n");
        goto end;
    }
    
    start_time=av_gettime();
    while (1) {
        AVStream *in_stream, *out_stream;
        //Get an AVPacket
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0) {
            break;
        }
        //FIX：No PTS (Example: Raw H.264)
        //Simple Write PTS
        if(pkt.pts==AV_NOPTS_VALUE){
            //Write PTS
            AVRational time_base1=ifmt_ctx->streams[videoindex]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
            //Parameters
            pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            pkt.dts=pkt.pts;
            pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
        }
        //Important:Delay
        if(pkt.stream_index==videoindex){
            AVRational time_base=ifmt_ctx->streams[videoindex]->time_base;
            AVRational time_base_q={1,AV_TIME_BASE};
            int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time) {
                av_usleep((int)(pts_time - now_time));
            }
        }
        
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
            printf("Send %8d %8lld video frames to output %s\n",frame_index, pkt.pts, _output.UTF8String);
            frame_index++;
        }
        if (pkt.stream_index == audioIndex) {
            printf("Send %8d audio frames to output %s\n",pcm_Index, _output.UTF8String);
            pcm_Index++;
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
    //写文件尾（Write file trailer）
    av_write_trailer(ofmt_ctx);
end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf( "Error occurred.\n");
        return;
    }
    return;
    
}
@end
