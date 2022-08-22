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
#include "FFAVDemuxer.h"
#include <thread>

namespace mogic {

class RTMPPushLive {
public:
    RTMPPushLive();
    ~RTMPPushLive();
    int initRTMP(int srcWidth, int srcHeight, AVPixelFormat srcFormat, const char *outPath, const char *audioPath);
    void setAudioLive(const char *outPath);

    // rgba
    int encodeByteData(uint8_t *byteData, uint32_t frameIndex);

    int encodeTail();

private:
    bool isConnected = false;
    int initContext();

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
    AVBSFContext *bsf_ctx;
    AVBitStreamFilterContext *bsfc;

    void destroy();
    FFVideoInfoPush videoInfo;
    AVFormatContext *ofmtCtx = nullptr;
    AVStream *videoStream = nullptr;
    AVStream *audioStream = nullptr;
    AVStream *inStream = nullptr;
    AVCodecContext *h264CodecCtx = nullptr;
    AVCodecContext *aacCodecCtx = nullptr;
    AVPacket *packet;

    // convert
    AVFrame *yuv420pFrame = nullptr;
    uint8_t *yuv420pBuffer = nullptr;
    SwsContext *swsContext = nullptr;

    volatile int exit = 0;
    int audioIndex = 0;

    static int interrupt_cb(void *ctx);

    FFAVDemuxer *demuxer = nullptr;
    AVStream *initAudioStream(int audioSampleRate, int audioChannels, int audioBitRate, AVStream *src);
    AVStream *initVideoStream();
    void pushAudio(int frameIndex, int64_t nowTime);
    bool hasAudio = false;
};

} // namespace mogic
#endif /* RTMPPushLive_hpp */
