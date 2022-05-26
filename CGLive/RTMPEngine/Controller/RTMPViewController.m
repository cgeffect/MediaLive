//
//  RTMPViewController.m
//  FFmpegiOS
//
//  Created by Jason on 2021/4/13.
//  Copyright © 2021 Jason. All rights reserved.
//

#import "RTMPViewController.h"
#import "CCSystemCapture.h"
#import "CCAudioEncoder.h"
#import "CCVideoEncoder.h"
#import "CCAVConfig.h"

#import "StreamPushController.h"
#import "LivePublisher.h"
#import "RTMPDumpController.h"

@interface RTMPViewController ()<CCSystemCaptureDelegate,CCVideoEncoderDelegate, CCAudioEncoderDelegate, LivePublisherDelegate>

@property (nonatomic, strong) CCSystemCapture *capture;
@property (nonatomic, strong) CCVideoEncoder *videoEncoder;
@property (nonatomic, strong) CCAudioEncoder *audioEncoder;
@property (nonatomic, strong) LivePublisher *livePublisher;
@property (nonatomic, assign) Float64 miliseconds;
@end

@implementation RTMPViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.whiteColor;

    [self initVideo];
}

#pragma mark - Video Test
- (void)initVideo {
    [CCSystemCapture checkCameraAuthor];
    
    _capture = [[CCSystemCapture alloc] initWithType:CCSystemCaptureTypeAll];//这是我只捕获了视频
    CGSize size = CGSizeMake(self.view.frame.size.width/2, self.view.frame.size.height/2);
    [_capture prepareWithPreviewSize:size];  //捕获视频时传入预览层大小
    _capture.preview.frame = CGRectMake(0, 150, size.width, size.height);
    [self.view addSubview:_capture.preview];
    self.capture.delegate = self;
    
    CCVideoConfig *config = [CCVideoConfig defaultConifg];
    config.width = _capture.witdh;
    config.height = _capture.height;
    config.bitrate = config.height * config.width * 4;
    
    _videoEncoder = [[CCVideoEncoder alloc] initWithConfig:config];
    _videoEncoder.delegate = self;
    
    CCAudioConfig *audioConf = [CCAudioConfig defaultConifg];
    audioConf.isNeedADTS = YES;
    _audioEncoder = [[CCAudioEncoder alloc] initWithConfig:audioConf];
    _audioEncoder.delegate = self;
}

- (IBAction)startCapture {
    CCVideoConfig *config = [CCVideoConfig defaultConifg];
    //rtmp://192.168.61.136/live/livestream
    //rtmp://192.168.0.5/live/livestream
    _livePublisher = [[LivePublisher alloc] initWithRTMPURL:@"rtmp://172.16.184.26:1935/live/livestream" videoWidth:config.width videoHeight:config.height videoFrameRate:config.fps videoBitRate:config.width * config.height * 4 audioSampleRate:44100 audioChannels:1 audioBitRate:6400 audioCodecName:@"libfdk_aac"];
    _livePublisher.delegate = self;
    
     [self.capture start];
}

- (IBAction)stopCapture {
    [self.capture stop];
}

#pragma mark--delegate
- (void)captureSampleBuffer:(CMSampleBufferRef)sampleBuffer type: (CCSystemCaptureType)type {
    if (type == CCSystemCaptureTypeAudio) {
        [_audioEncoder encodeAudioSamepleBuffer:sampleBuffer];
    }else {
        [_videoEncoder encodeVideoSampleBuffer:sampleBuffer];
    }
}

#pragma mark--CCVideoEncoder/Decoder Delegate
- (void)videoEncodeCallbacksps:(NSData *)sps pps:(NSData *)pps {
    uint8_t *nalu = (uint8_t *)sps.bytes;
    for (int i = 0; i < sps.length; i++) {
        printf("%02x ", nalu[i]);
    }
    printf("\n");
    int startCodeLength = 4;
    int spsLength = (int)sps.length - startCodeLength;
    int ppsLength = (int)pps.length - startCodeLength;
    NSData *spsFrame = [NSData dataWithBytes:sps.bytes + startCodeLength length:spsLength];
    NSData *ppsFrame = [NSData dataWithBytes:pps.bytes + startCodeLength length:ppsLength];
    uint8_t *nalu1 = (uint8_t *)spsFrame.bytes;
    for (int i = 0; i < spsFrame.length; i++) {
        printf("%02x ", nalu1[i]);
    }
    printf("\n");
    uint8_t *nalu2 = (uint8_t *)ppsFrame.bytes;
    for (int i = 0; i < ppsFrame.length; i++) {
        printf("%02x ", nalu2[i]);
    }
    printf("\n");

    [_livePublisher pushSpsPps:spsFrame pps:ppsFrame timestramp:0];
}

- (void)videoEncodeCallback:(NSData *)h264Data isKeyFrame:(NSInteger)isKeyFrame {
//    uint8_t *nalu1 = (uint8_t *)h264Data.bytes;
//    for (int i = 0; i < h264Data.length; i++) {
//        printf("%02x ", nalu1[i]);
//    }
//    printf("\n");
    [_livePublisher pushVideoData:h264Data isKeyFrame:isKeyFrame timestramp:_miliseconds];
    _miliseconds += 40;
}

#pragma mark--CCAudioEncoder/Decoder Delegate
- (void)audioEncodeCallback:(NSData *)aacData {
    //[_livePublisher pushAudioData:aacData sampleRate:44100 startRecordTimeMills:CFAbsoluteTimeGetCurrent() * 1000];
}

#pragma mark LivePublisherDelegate
- (void)onConnectSuccess {
    
}
- (void)onConnectFailed {
    
}
- (void)publishTimeOut {
    
}



- (IBAction)test {
    UIStoryboard *sb = [UIStoryboard storyboardWithName:@"RTMP" bundle:nil];
    StreamPushController *vc = [sb instantiateViewControllerWithIdentifier:@"streamPushController"];
    [self.navigationController pushViewController:vc animated:YES];
}

- (IBAction)rtmp_lib:(id)sender {
    RTMPDumpController *vc = [[RTMPDumpController alloc] init];
    [self.navigationController pushViewController:vc animated:YES];
}
@end
