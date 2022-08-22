//
//  RTMPDumpController.m
//  FFmpegiOS
//
//  Created by Jason on 2021/4/19.
//  Copyright © 2021 Jason. All rights reserved.
//

#import "RTMPDumpController.h"
#import "simplest_librtmp_send264.h"
#import "simplest_librtmp_sendflv.h"
#include "simplest_librtmp_receive.h"

@interface RTMPDumpController ()

@end

@implementation RTMPDumpController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.view.backgroundColor = UIColor.whiteColor;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
//    NSString* inPath = [[NSBundle mainBundle] pathForResource:@"cuc_ieschool" ofType:@"h264"];
//    load_file((char *)inPath.UTF8String);

    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_global_queue(0, DISPATCH_QUEUE_PRIORITY_DEFAULT), ^{
        NSString* inPathflv = [[NSBundle mainBundle] pathForResource:@"source.200kbps.768x320" ofType:@"flv"];
        send_flv((char *)inPathflv.UTF8String);
    });

    //延迟2秒后在开一个新的子线程执行block中代码
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_global_queue(0, DISPATCH_QUEUE_PRIORITY_DEFAULT), ^{
        NSString *flv = [self creatFile:@"received.flv"];
        received_flv1((char *)flv.UTF8String);
    });
    
}

- (NSString *)creatFile:(NSString *)fileName {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *path = [paths objectAtIndex:0];
    NSString *tmpPath = [path stringByAppendingPathComponent:@"temp"];
    NSTimeInterval time = [[NSDate date] timeIntervalSince1970];
    [[NSFileManager defaultManager] createDirectoryAtPath:tmpPath withIntermediateDirectories:YES attributes:nil error:NULL];
    NSString* outFilePath = [tmpPath stringByAppendingPathComponent:[NSString stringWithFormat:@"%d_%@", (int)time, fileName]];
    return outFilePath;
}


@end
