//
//  RTMPPush.hpp
//  mogic
//
//  Created by Jason on 2022/5/26.
//

#ifndef RTMPPush_hpp
#define RTMPPush_hpp

#include <stdio.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}

namespace mogic {
class RTMPPush {
public:
    int initRTMP(const char *outPath, const char *filePath);
    void readPacket();
private:
    int videoIndex = -1;
    int frameIndex = 0;
    int audioIndex = -1;
    int pcmIndex = 0;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmtCtx = NULL, *ofmtCtx = NULL;
    AVOutputFormat *ofmt = NULL;
    int64_t startTime=0;
    const char *outPath;
    int writerHeader();
    int writerPacket(AVPacket *pkt);
    int writerTrailer();
    void stop();
};
}
#endif /* RTMPPush_hpp */
