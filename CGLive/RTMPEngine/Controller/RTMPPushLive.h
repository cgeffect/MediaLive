//
//  RTMPPushLive.hpp
//  CGLive
//
//  Created by Jason on 2022/5/27.
//

#ifndef RTMPPushLive_hpp
#define RTMPPushLive_hpp

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace mogic {

class RTMPPushLive {
public:
    RTMPPushLive();
    ~RTMPPushLive();
    int initRTMP(int srcWidth, int srcHeight, AVPixelFormat srcFormat, const char *outPath);
    void setAudioLive(const char *outPath);

    int initCodecCtx();
    int encodeByteData(uint8_t *byteData, uint32_t frameIndex);
    int encodeTail();

private:
    struct FFVideoInfoPush {
        int srcWidth;
        int srcHeight;
        AVPixelFormat srcPixFmt;

        int dstWidth;
        int dstHeight;
        AVPixelFormat dstPixFmt;

        const char *outPath;
        long bitRate;
        float dstFps;
    };

    int encodeFrame(AVFrame *srcFrame);
    int64_t startTime = 0;

    void destroy();
    FFVideoInfoPush videoInfo;
    AVFormatContext *ofmtCtx = nullptr;
    AVStream *newStream = nullptr;
    AVCodecContext *codecCtx = nullptr;
    AVPacket *packet;

    // convert
    AVFrame *yuv420pFrame = nullptr;
    uint8_t *yuv420pBuffer = nullptr;
    SwsContext *swsContext = nullptr;

    volatile int exit = 0;
};

} // namespace mogic
#endif /* RTMPPushLive_hpp */
