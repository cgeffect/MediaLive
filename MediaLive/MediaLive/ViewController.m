//
//  ViewController.m
//  MediaLive
//
//  Created by 王腾飞 on 2023/10/23.
//

#import "ViewController.h"
#import "simplest_librtmp_send264.h"
#import "simplest_librtmp_sendflv.h"
#include "simplest_librtmp_receive.h"

//https://github.com/ossrs/librtmp

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    
    // Do any additional setup after loading the view.
}
- (IBAction)rtmppush:(NSButtonCell *)sender {
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_global_queue(0, DISPATCH_QUEUE_PRIORITY_DEFAULT), ^{
        NSString* inPathflv = [[NSBundle mainBundle] pathForResource:@"source.200kbps.768x320" ofType:@"flv"];
        librtmp_send_flv((char *)inPathflv.UTF8String);
    });

    //延迟2秒后在开一个新的子线程执行block中代码
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_global_queue(0, DISPATCH_QUEUE_PRIORITY_DEFAULT), ^{
//        NSString *flv = @"/Users/jason/Jason/project/MediaLive/MediaLive/MediaLive/received.flv";
        NSString *flv = [self creatFile:@"received.flv"];
        NSLog(@"%@", flv);
        received_flv1((char *)flv.UTF8String);
        
    });
}
- (IBAction)rtmp_push_h264:(NSButton *)sender {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_global_queue(0, DISPATCH_QUEUE_PRIORITY_DEFAULT), ^{
        NSString* inPathflv = [[NSBundle mainBundle] pathForResource:@"cuc_ieschool" ofType:@"h264"];
        librtmp_send_h264((char *)inPathflv.UTF8String);
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


- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
