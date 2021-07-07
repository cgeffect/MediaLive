//
//  LiveVideoPublisher.h
//  FFmpegiOS
//
//  Created by Jason on 2021/4/13.
//  Copyright © 2021 Jason. All rights reserved.
//

#ifndef LiveVideoPublisher_h
#define LiveVideoPublisher_h

#include <iostream>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/time.h>
    #include <libavutil/opt.h>
}

#define H264_NALU_TYPE_NON_IDR_PICTURE                                  1
#define H264_NALU_TYPE_IDR_PICTURE                                      5
#define H264_NALU_TYPE_SEQUENCE_PARAMETER_SET                           7
#define H264_NALU_TYPE_PICTURE_PARAMETER_SET                            8
#define H264_NALU_TYPE_SEI                                              6

#define NON_DROP_FRAME_FLAG                                             -1.0f
#define DTS_PARAM_UN_SETTIED_FLAG                                        -1
#define DTS_PARAM_NOT_A_NUM_FLAG                                        -2
#define PTS_PARAM_UN_SETTIED_FLAG                                        -1

typedef struct LiveVideoPacket {
    unsigned char *buffer;
    int size;
    int timeMills;
    int duration;
    int64_t pts;
    int64_t dts;
    
    LiveVideoPacket() {
        buffer = NULL;
        size = 0;
        pts = PTS_PARAM_UN_SETTIED_FLAG;
        dts = DTS_PARAM_UN_SETTIED_FLAG;
    }
    
    ~LiveVideoPacket() {
        if (NULL != buffer) {
            delete[] buffer;
            buffer = NULL;
        }
    }
    
    int getNALUType() {
        int nalu_type = H264_NALU_TYPE_NON_IDR_PICTURE;
        if (NULL != buffer) {
            nalu_type = buffer[4] & 0x1F;
        }
        return nalu_type;
    }
    
    bool isIDRFrame() {
        bool ret = false;
        if (getNALUType() == H264_NALU_TYPE_IDR_PICTURE) {
            ret = true;
        }
        return ret;
    }
    
    LiveVideoPacket* clone() {
        LiveVideoPacket *result = new LiveVideoPacket();
        result->buffer = new unsigned char[size];
        memcpy(result->buffer, buffer, size);
        result->size = size;
        result->timeMills = timeMills;
        return result;
    }
} LiveVideoPacket;

typedef struct LiveAudioPacket {
    short *buffer;
    unsigned char *data;
    int size;
    float position;
    long frameNum;
    
    LiveAudioPacket() {
        buffer = NULL;
        data = NULL;
        size = 0;
        position = -1;
    }
    
    ~LiveAudioPacket() {
        if (NULL != buffer) {
            delete[] buffer;
            buffer = NULL;
        }
        if (NULL != data) {
            delete[] data;
            data = NULL;
        }
    }
} LiveAudioPacket;

class LiveVideoPublisher {
public:
    
private:
    int publishTimeout{15 * 1000};
    long latestFrameTime{0}; //发出的最后一帧的发送时间
    long duration{0};
    long lastPresentationTimeMs;
    long lastAudioPacketPresentationTimeMills;
    long videoStreamTimeInSecs;
    bool isWriteHeaderSuccess{false};
    bool isConnected{false};
    AVFormatContext *oc = NULL;
    AVOutputFormat* fmt = NULL;

    AVStream *audio_st;
    AVStream *video_st;
    AVBitStreamFilterContext *bsfc;
    
    int index;
public:
    LiveVideoPublisher();
    ~LiveVideoPublisher();
    int init(char* videoOutputURI, int videoWidth,
            int videoHeight,float videoFrameRate,int videoBitRate,
            int audioSampleRate, int audioChannels, int audioBitRate,
             char* audio_codec_name);
    
    int pushVideoPacket(LiveVideoPacket *packet);
    int pushAudioPacket(LiveAudioPacket *packet);
    void stop();
    
private:
    double getVideoStreamTimeInSecs();
    double getAudioStreamTimeInSecs();
    static int interrupt_cb(void *ctx);
    int detectTimeout();

    //
    void parseH264SequenceHeader(uint8_t *in_pBuffer, uint32_t in_ui32Size, uint8_t **inout_pBufferSPS, int &inout_ui32sizeSPS, uint8_t **inout_pBufferPPS, int &inout_ui32sizePPS);
    uint32_t findStartCode(uint8_t *in_pBuffer, uint32_t in_ui32BufferSize, uint32_t in_ui32Code, uint32_t &out_ui32ProcessedBytes);

    AVStream* buildVideoStream(AVFormatContext *oc, int videoWidth, int videoHeight, float videoFrameRate, int videoBitRate);
    AVStream* buildAudioStream(AVFormatContext *oc, int audioSampleRate, int audioChannels, int audioBitRate, char *audio_codec_name);

    int write_video_frame(AVFormatContext *oc, AVStream *st, LiveVideoPacket *packet);
    int write_audio_frame(AVFormatContext *oc, AVStream *st, LiveAudioPacket *packet);
};
#endif /* LiveVideoPublisher_h */
