//
//  PullRTMP.m
//  CGLive
//
//  Created by 王腾飞 on 2021/11/10.
//

#import "PullRTMP.h"
#import "AVVideoDecoderFF.h"

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
@end
