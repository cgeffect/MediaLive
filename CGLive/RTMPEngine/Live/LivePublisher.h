//
//  LivePublisher.h
//  FFmpegiOS
//
//  Created by Jason on 2021/4/13.
//  Copyright © 2021 Jason. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudioTypes.h>

NS_ASSUME_NONNULL_BEGIN

@protocol LivePublisherDelegate <NSObject>

@required
- (void)onConnectSuccess;
- (void)onConnectFailed;
- (void)publishTimeOut;

@end

@interface LivePublisher : NSObject
@property (nonatomic, weak) id<LivePublisherDelegate> delegate;
@property (nonatomic, assign) double startConnectTimeMills;

- (instancetype)initWithRTMPURL:(NSString *)rtmpURL
                     videoWidth:(NSInteger)videoWidth videoHeight:(NSInteger)videoHeight
                 videoFrameRate:(NSInteger)videoFrameRate videoBitRate:(NSInteger)videoBitRate
                audioSampleRate:(NSInteger)audioSampleRate audioChannels:(NSInteger)audioChannels
                   audioBitRate:(NSInteger)audioBitRate audioCodecName:(NSString *)audioCodecName;

- (void)pushSpsPps:(NSData*)sps pps:(NSData*)pps timestramp:(Float64)miliseconds;
- (void)pushVideoData:(NSData*)data isKeyFrame:(BOOL)isKeyFrame timestramp:(Float64)miliseconds;
- (void)pushAudioData:(NSData *)data sampleRate:(int)sampleRate startRecordTimeMills:(Float64)startRecordTimeMills;
- (void)start;
- (void)stop;
@end

NS_ASSUME_NONNULL_END
