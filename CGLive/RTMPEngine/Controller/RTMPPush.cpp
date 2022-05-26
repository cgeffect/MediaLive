//
//  RTMPPush.cpp
//  CGLive
//
//  Created by Jason on 2022/5/26.
//

#include "RTMPPush.h"

namespace mogic {
int RTMPPush::initRTMP(const char *outPath, const char *filePath) {
    this->outPath = outPath;
    int ret;

    avformat_network_init();
    //Input
    if ((ret = avformat_open_input(&ifmtCtx, filePath, 0, 0)) < 0) {
        printf( "Could not open input file.");
        stop();
        return -1;
    }
    printf( "in_filename: %s\n", filePath);
    if ((ret = avformat_find_stream_info(ifmtCtx, 0)) < 0) {
        printf( "Failed to retrieve input stream information");
        stop();
        return -1;
    }
    printf( "out_filename: %s\n", outPath);
    
    for(int i = 0; i < ifmtCtx->nb_streams; i++) {
        AVStream *stream = ifmtCtx->streams[i];
        AVCodecParameters *param = stream->codecpar;
        if (param->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex=i;
            break;
        }
    }
    for(int i = 0; i < ifmtCtx->nb_streams; i++) {
        if (ifmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIndex = i;
            break;
        }
    }
    
    av_dump_format(ifmtCtx, 0, filePath, 0);

    //Output
    
    avformat_alloc_output_context2(&ofmtCtx, NULL, "flv", outPath); //RTMP
    //avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//UDP
    
    if (!ofmtCtx) {
        printf( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        stop();
        return -1;
    }
    
    ofmt = ofmtCtx->oformat;
    
    printf("name:%s, long_name:%s, mime_type:%s, extensions:%s\n", ofmt->name, ofmt->long_name, ofmt->mime_type, ofmt->extensions);
    
    for (int i = 0; i < ifmtCtx->nb_streams; i++) {
        AVStream *inStream = ifmtCtx->streams[i];
        if(inStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO && inStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        AVCodec *inCodec = avcodec_find_decoder(inStream->codecpar->codec_id);
        AVStream *outStream = avformat_new_stream(ofmtCtx, NULL);
        if (!outStream) {
            printf( "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            stop();
            return -1;
        }
        AVCodecContext *codecCtx = avcodec_alloc_context3(inCodec);
        ret = avcodec_parameters_to_context(codecCtx, inStream->codecpar);
        if (ret < 0){
            printf("Failed to copy in_stream codecpar to codec context\n");
            stop();
            return -1;
        }
         
        codecCtx->codec_tag = 0;
        if (ofmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_parameters_from_context(outStream->codecpar, codecCtx);
        if (ret < 0){
            printf("Failed to copy codec context to out_stream codecpar context\n");
            stop();
            return -1;
        }
    }
    
    av_dump_format(ofmtCtx, 0, outPath, 1);
    //Open output URL
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        AVDictionary *format_opts = NULL;
        av_dict_set(&format_opts, "rw_timeout",  "1000000", 0); //设置超时时间,单位mcs
        avio_open2( &ofmtCtx->pb, outPath , AVIO_FLAG_WRITE , NULL , &format_opts);
//        ret = avio_open(&ofmtCtx->pb, outPath, AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf( "Could not open output URL '%s'", outPath);
            stop();
            return -1;
        }
    }

    readPacket();
    return 0;
}

void RTMPPush::readPacket() {
    int ret;
    ret = writerHeader();
    if (ret < 0) {
        printf( "Error occurred when opening output URL\n");
        stop();
        return;
    }
    
    startTime = av_gettime();

    while (true) {
        //每次都用新的, 重复使用同一个会偶现av_interleaved_write_frame卡死
        AVPacket pkt = {0};
        ret = av_read_frame(ifmtCtx, &pkt);
        if (ret < 0) {
            break;
        }

        ret = writerPacket(pkt);
        if (ret < 0) {
            printf( "Error muxing packet\n");
            break;
        }
//        av_packet_unref(&pkt);
    }
//    av_packet_free(&pkt);
    writerTrailer();
    
    stop();
    return;
}


int RTMPPush::writerHeader() {
    int ret = avformat_write_header(ofmtCtx, NULL);
    return ret;
}

int RTMPPush::writerTrailer() {
    //写文件尾（Write file trailer）
    int ret = av_write_trailer(ofmtCtx);
    return ret;
}

int RTMPPush::writerPacket(AVPacket pkt) {
    int ret = 0;

    //FIX：No PTS (Example: Raw H.264)
    //Simple Write PTS
    if(pkt.pts == AV_NOPTS_VALUE) {
        //Write PTS
        AVRational time_base1 = ifmtCtx->streams[videoIndex]->time_base;
        //Duration between 2 frames (us)
        int64_t calc_duration = (double)AV_TIME_BASE/av_q2d(ifmtCtx->streams[videoIndex]->r_frame_rate);
        //Parameters
        pkt.pts = (double)(frameIndex*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
        pkt.dts = pkt.pts;
        pkt.duration = (double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
    }
    
    //Important:Delay
    if(pkt.stream_index == videoIndex) {
        AVRational timeBase = ifmtCtx->streams[videoIndex]->time_base;
        AVRational timeBaseQ = {1, AV_TIME_BASE};
        int64_t ptsTime = av_rescale_q(pkt.dts, timeBase, timeBaseQ);
        int64_t nowTime = av_gettime() - startTime;
        if (ptsTime > nowTime) {
            av_usleep((int)(ptsTime - nowTime));
        }
    }

    AVStream *inStream  = ifmtCtx->streams[pkt.stream_index];
    AVStream *outStream = ofmtCtx->streams[pkt.stream_index];
    /* copy packet */
    //Convert PTS/DTS
    pkt.pts = av_rescale_q_rnd(pkt.pts, inStream->time_base, outStream->time_base, AVRounding(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt.dts = av_rescale_q_rnd(pkt.dts, inStream->time_base, outStream->time_base, AVRounding(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    pkt.duration = av_rescale_q(pkt.duration, inStream->time_base, outStream->time_base);
    pkt.pos = -1;
    
    if(pkt.stream_index==videoIndex){
        printf("Send %8d %8lld video frames to output %s\n",frameIndex, pkt.pts, outPath);
        frameIndex++;
    }
    if (pkt.stream_index == audioIndex) {
        printf("Send %8d audio frames to output %s\n",pcmIndex, outPath);
        pcmIndex++;
    }
    
    //ret = av_write_frame(ofmt_ctx, &pkt);
    ret = av_interleaved_write_frame(ofmtCtx, &pkt);

    if (ret < 0) {
        printf( "Error muxing packet\n");
    }
    return ret;
}

void RTMPPush::stop() {
    printf("RTMPPush::stop\n");
    if (ifmtCtx) {
        avformat_close_input(&ifmtCtx);
        ifmtCtx = nullptr;
    }
    /* close output */
    if (ofmtCtx) {
        if (ofmtCtx && !(ofmt->flags & AVFMT_NOFILE)) {
            avio_close(ofmtCtx->pb);
        }
        avformat_free_context(ofmtCtx);
        ofmtCtx = nullptr;
    }
    
//    if (ret < 0 && ret != AVERROR_EOF) {
//        printf( "Error occurred.\n");
//        return;
//    }
}

}
