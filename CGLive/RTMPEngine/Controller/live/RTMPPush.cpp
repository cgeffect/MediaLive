//
//  RTMPPush.cpp
//  CGLive
//
//  Created by Jason on 2022/5/26.
//

#include "RTMPPush.h"
#include "MogicDefines.h"

namespace mogic {
/* avio_open2函数的返回值大于等于0，则将 isConnected变量设置为true，代表其已经成功地打开了文件输出通道。 唯一需要注意的一点是，需要配置一个超时回调函数进去，这个回调函 数主要是给FFmpeg的协议层用的，在实现这个函数的时候，返回1则代 表结束I/O操作，返回0则代表继续I/O操作 */
int RTMPPush::interrupt_cb(void *ctx) { // 超时回调函数
//    LiveVideoPublisher *publisher = (LiveVideoPublisher *)ctx;
//    long diff = platform_4_live::getCurrentTimeMills() - publisher->latestFrameTime;
//    if (diff > publisher->publishTimeout) {
////        int queueSize = LivePacketPool::GetInstance()->getRecordingVideoPacketQueueSize();
//        printf("LiveVideoPublisher::interrupt_cb callback time out ... queue size:%ld\n", diff);
//        return 1; // 返回 1 则代表结束 I/O 操作
//    }
    return 0;
}
int RTMPPush::initRTMP(const char *outPath, const char *filePath) {
    this->outPath = outPath;
    int ret;

    avformat_network_init();
    //Input
    if ((ret = avformat_open_input(&ifmtCtx, filePath, 0, 0)) < 0) {
        MOGIC_DLOG("Could not open input file %s", filePath);
        stop();
        return -1;
    }
    if ((ret = avformat_find_stream_info(ifmtCtx, 0)) < 0) {
        MOGIC_DLOG( "Failed to retrieve input stream information");
        stop();
        return -1;
    }
    
    for(unsigned int i = 0; i < ifmtCtx->nb_streams; i++) {
        AVStream *stream = ifmtCtx->streams[i];
        AVCodecParameters *param = stream->codecpar;
        if (param->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
        }
        if (param->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIndex = i;
        }
    }
    
    av_dump_format(ifmtCtx, 0, filePath, 0);

    //Output
    avformat_alloc_output_context2(&ofmtCtx, NULL, "flv", outPath); //RTMP
    //avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);//UDP
    
    if (!ofmtCtx) {
        MOGIC_DLOG( "Could not create output context");
        ret = AVERROR_UNKNOWN;
        stop();
        return -1;
    }
    
    ofmt = ofmtCtx->oformat;
    
    MOGIC_DLOG("name:%s, long_name:%s, mime_type:%s, extensions:%s", ofmt->name, ofmt->long_name, ofmt->mime_type, ofmt->extensions);
    
    for (unsigned int i = 0; i < ifmtCtx->nb_streams; i++) {
        AVStream *inStream = ifmtCtx->streams[i];
        if(inStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO && inStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        AVCodec *inCodec = avcodec_find_encoder(inStream->codecpar->codec_id);
        AVStream *outStream = avformat_new_stream(ofmtCtx, NULL);
        if (!outStream) {
            MOGIC_DLOG( "Failed allocating output stream");
            ret = AVERROR_UNKNOWN;
            stop();
            return -1;
        }
        AVCodecContext *codecCtx = avcodec_alloc_context3(inCodec);
        ret = avcodec_parameters_to_context(codecCtx, inStream->codecpar);
        if (ret < 0){
            MOGIC_DLOG("Failed to copy in_stream codecpar to codec context");
            stop();
            return -1;
        }
         
        codecCtx->codec_tag = 0;
        if (ofmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_parameters_from_context(outStream->codecpar, codecCtx);
        if (ret < 0){
            MOGIC_DLOG("Failed to copy codec context to out_stream codecpar context");
            stop();
            return -1;
        }
    }
    
    av_dump_format(ofmtCtx, 0, outPath, 1);
    
    //打开连接通道
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        AVDictionary *format_opts = NULL;
        av_dict_set(&format_opts, "rw_timeout",  "1000000", 0); //设置超时时间,单位mcs
        
        AVIOInterruptCB int_cb = { interrupt_cb, this};
        ofmtCtx->interrupt_callback = int_cb;
        int ret = avio_open2(&ofmtCtx->pb, outPath, AVIO_FLAG_WRITE, &ofmtCtx->interrupt_callback, &format_opts);
        if (ret < 0) {
            printf("Could not open '%s': %s\n", outPath, av_err2str(ret));
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
        MOGIC_DLOG( "Error occurred when opening output URL");
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
            MOGIC_DLOG( "Error muxing packet");
            break;
        }
//        av_packet_unref(&pkt);
    }
//    av_packet_free(&pkt);
    writerTrailer();
    
    stop();
    return;
}

int RTMPPush::pushPacket(AVPacket *pkt1) {
    if (startTime == 0) {
        startTime = av_gettime();
        int ret = writerHeader();
        if (ret < 0) {
            MOGIC_DLOG( "Error occurred when opening output URL");
            stop();
            return -1;
        }
    }
    AVPacket pkt = *pkt1;
    int ret = writerPacket(pkt);
    if (ret < 0) {
        MOGIC_DLOG( "Error muxing packet");
        return -1;
    }
    return 0;
}

void RTMPPush::stopPush() {
    writerTrailer();
    stop();
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
        MOGIC_DLOG("send video index: %8d pts: %8lld", frameIndex, pkt.pts);
        frameIndex++;
    }
    if (pkt.stream_index == audioIndex) {
        MOGIC_DLOG("send audio index: %8d pts: %8lld", pcmIndex, pkt.pts);
        pcmIndex++;
    }
    
    //ret = av_write_frame(ofmt_ctx, &pkt);
    ret = av_interleaved_write_frame(ofmtCtx, &pkt);

    if (ret < 0) {
        MOGIC_DLOG("Error muxing packet");
    }
    return ret;
}

void RTMPPush::stop() {
    MOGIC_DLOG("RTMPPush::stop");
    if (ifmtCtx) {
        avformat_close_input(&ifmtCtx);
        ifmtCtx = nullptr;
    }
    /* close output */
    if (ofmtCtx) {
        if (!(ofmt->flags & AVFMT_NOFILE)) {
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
