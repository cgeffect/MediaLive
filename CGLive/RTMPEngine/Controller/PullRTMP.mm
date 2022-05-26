//
//  PullRTMP.m
//  CGLive
//
//  Created by 王腾飞 on 2021/11/10.
//

#import "PullRTMP.h"
#import "AVVideoDecoderFF.h"
#include "RTMPPush.h"

@interface PullRTMP ()
{
    AVVideoDecoderFF *_dcFF;
}

@end

@implementation PullRTMP
- (instancetype)init {
    self = [super init];
    if (self) {
        _dcFF = new AVVideoDecoderFF();
    }
    return self;
}

- (void)loadPath {
    NSString * _output = @"rtmp://192.168.0.8/live/livestream";
    _dcFF->loadResource(_output.UTF8String);
    while (true) {
        _dcFF->decodeFrame();
        usleep(30 * 1000);
    }
}

- (void)push {
    NSString *input_nsstr= [[NSBundle mainBundle] pathForResource:@"source.200kbps.768x320" ofType:@"flv"];
    mogic::RTMPPush push;
    push.initRTMP("rtmp://172.16.184.26:1935/live/livestream", input_nsstr.UTF8String);
}

@end
