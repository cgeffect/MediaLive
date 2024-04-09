//
//  AVVideoDecoderr.hpp
//  FFmpegiOS
//
//  Created by Jason on 2020/11/11.
//  Copyright © 2020 Jason. All rights reserved.
//

#ifndef AVVideoDecoder_hpp
#define AVVideoDecoder_hpp

//cpp文件使用c文件, 使用宏定义告诉编译器以C的方式编译C文件
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif

#include <stdio.h>
#include <pthread.h>
#include "AVVideoDemuxer.h"

using namespace std;
class AVVideoDecoderFF {
public:
    AVVideoDecoderFF();
    ~AVVideoDecoderFF();
    
    void loadResource(const char *path);
    void decodeFrame();
    void seekTo(float seekMs);
    void destroy();
    void activateHwAccel();
    void destroyHwAccel();
    inline bool isEndOfFile() {
        return isEOF;
    }

private:
    AVFormatContext *avformat_context{nullptr};
    AVCodecContext *avcodec_context{nullptr};
    AVCodec *video_codec{nullptr};
    AVCodec *audio_codec{nullptr};
    AVStream *video_stream{nullptr};
    AVVideoDemuxer av_demuxer{nullptr};
    int video_stream_index{-1};
    int audio_stream_index{-1};
    bool isEOF{false};

private:
    const char *filePath{nullptr};
    void decoderVideo();
    void decoderPacket(AVCodecContext *dec_ctx, AVPacket *pkt, AVFrame *frame);
    
    float getDurationSEC() {
        if (!avformat_context) {
            return 0;
        }
        if (avformat_context->duration == AV_NOPTS_VALUE) {
            return -1;
        }
        return 1.0f * avformat_context->duration / AV_TIME_BASE;
    }
};

#endif /* AVVideoDecoder_hpp */
