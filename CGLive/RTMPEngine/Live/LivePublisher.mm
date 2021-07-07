//
//  LivePublisher.m
//  FFmpegiOS
//
//  Created by Jason on 2021/4/13.
//  Copyright © 2021 Jason. All rights reserved.
//

#import "LivePublisher.h"
#import "LiveVideoPublisher.h"

@interface LivePublisher ()
{
    LiveVideoPublisher *_videoPublisher;
}
@property (nonatomic, strong) NSString *rtmpURL;
@property (nonatomic, assign) NSInteger videoWidth;
@property (nonatomic, assign) NSInteger videoHeight;
@property (nonatomic, assign) NSInteger videoFrameRate;
@property (nonatomic, assign) NSInteger videoBitRate;
@property (nonatomic, assign) NSInteger audioSampleRate;
@property (nonatomic, assign) NSInteger audioChannels;
@property (nonatomic, assign) NSInteger audioBitRate;
@property (nonatomic, strong) NSString *audioCodecName;

@property (nonatomic, assign) int totalSampleCount;

@end

@implementation LivePublisher

- (instancetype)initWithRTMPURL:(NSString *)rtmpURL videoWidth:(NSInteger)videoWidth videoHeight:(NSInteger)videoHeight videoFrameRate:(NSInteger)videoFrameRate videoBitRate:(NSInteger)videoBitRate audioSampleRate:(NSInteger)audioSampleRate audioChannels:(NSInteger)audioChannels audioBitRate:(NSInteger)audioBitRate audioCodecName:(NSString *)audioCodecName {
    self = [super init];
    if (self) {
        self.rtmpURL = rtmpURL;
        self.videoWidth = videoWidth;
        self.videoHeight = videoHeight;
        self.videoFrameRate = videoFrameRate;
        self.videoBitRate = videoBitRate;
        self.audioSampleRate = audioSampleRate;
        self.audioChannels = audioChannels;
        self.audioBitRate = audioBitRate;
        self.audioCodecName = audioCodecName;
        
        _videoPublisher = new LiveVideoPublisher();
        _videoPublisher->init((char *)rtmpURL.UTF8String, (int)videoWidth, (int)videoHeight, (int)videoFrameRate, (int)videoBitRate, (int)audioSampleRate, (int)audioChannels, (int)audioBitRate, (char *)audioCodecName.UTF8String);
    }
    return self;
}

- (void)pushSpsPps:(NSData *)sps pps:(NSData *)pps timestramp:(Float64)miliseconds {
    const char bytesHeader[] = "\x00\x00\x00\x01";
    size_t headerLength = 4;
    
    LiveVideoPacket *videoPacket = new LiveVideoPacket();
    size_t length = 2 * headerLength + sps.length + pps.length;
    videoPacket->buffer = new unsigned char[length];
    videoPacket->size = int(length);
    memcpy(videoPacket->buffer, bytesHeader, headerLength);
    memcpy(videoPacket->buffer + headerLength, (unsigned char*)[sps bytes], sps.length);
    memcpy(videoPacket->buffer + headerLength + sps.length, bytesHeader, headerLength);
    memcpy(videoPacket->buffer + headerLength * 2 + sps.length, (unsigned char*)[pps bytes], pps.length);
    videoPacket->timeMills = 0;
    
    uint8_t *nalu1 = (uint8_t *)videoPacket->buffer;
    for (int i = 0; i < videoPacket->size; i++) {
        printf("%02x ", nalu1[i]);
    }
    printf("\n");
    
    _videoPublisher->pushVideoPacket(videoPacket);
    
}

- (void)pushVideoData:(NSData *)data isKeyFrame:(BOOL)isKeyFrame timestramp:(Float64)miliseconds {
    LiveVideoPacket *videoPacket = new LiveVideoPacket();
    videoPacket->buffer = new unsigned char[data.length];
    videoPacket->size = int(data.length);
    memcpy(videoPacket->buffer, [data bytes], data.length);
    videoPacket->timeMills = miliseconds;
    
    _videoPublisher->pushVideoPacket(videoPacket);
}

- (void)pushAudioData:(NSData *)data sampleRate:(int)sampleRate startRecordTimeMills:(Float64)startRecordTimeMills {
    double maxDiffTimeMills = 25;
    double minDiffTimeMills = 10;
    double audioSamplesTimeMills = CFAbsoluteTimeGetCurrent() * 1000 - startRecordTimeMills;
    int audioSampleRate = sampleRate;
    int audioChannels = 2;
    double dataAccumulateTimeMills = self.totalSampleCount * 1000 / audioSampleRate / audioChannels;
    if (dataAccumulateTimeMills <= audioSamplesTimeMills - maxDiffTimeMills) {
        double correctTimeMills = audioSamplesTimeMills - dataAccumulateTimeMills - minDiffTimeMills;
        int correctBufferSize = (int)(correctTimeMills / 1000.0 * audioSampleRate * audioChannels);
        LiveAudioPacket *audioPacket = new LiveAudioPacket();
        audioPacket->buffer = new short[correctBufferSize];
        memset(audioPacket->buffer, 0, correctBufferSize * sizeof(short));
        audioPacket->size = correctBufferSize;
//        LivePacketPool::GetInstance()->pushAudioPacketToQueue(audioPacket);
        _videoPublisher->pushAudioPacket(audioPacket);
        self.totalSampleCount += correctBufferSize;
        NSLog(@"Correct Time Mills is %lf\n", correctTimeMills);
        NSLog(@"audioSamplesTimeMills is %lf, dataAccumulateTimeMills is %lf\n", audioSamplesTimeMills, dataAccumulateTimeMills);
    }
    int sampleCount = (int)data.length / 2;
    self.totalSampleCount += sampleCount;
    short *packetBuffer = new short[sampleCount];
    memcpy(packetBuffer, data.bytes, data.length);
    LiveAudioPacket *audioPacket = new LiveAudioPacket();
    audioPacket->data = (unsigned char *)data.bytes;
    audioPacket->buffer = packetBuffer;
    audioPacket->size = sampleCount;
//    LivePacketPool::GetInstance()->pushAudioPacketToQueue(audioPacket);
    _videoPublisher->pushAudioPacket(audioPacket);
}

- (void)start {
    
}

- (void)stop {
    
}

@end
