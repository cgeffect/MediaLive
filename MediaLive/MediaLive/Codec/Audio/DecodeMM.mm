//
//  DecodeMM.m
//  MediaLive
//
//  Created by Jason on 2024/4/9.
//

#import "DecodeMM.h"
#include "decode_filter_audio.hpp"
#include "video_filter.hpp"
#include "video_filter_decode.hpp"

@implementation DecodeMM

- (void)video_filter_decode:(NSString *)in0 in1:(NSString *)in1 ou:(NSString *)ou{
    video_filter_decode(in0.UTF8String, in1.UTF8String, ou.UTF8String);

}

- (void)video_filter:(NSString *)in0 ou:(NSString *)ou {
    video_filter(in0.UTF8String, ou.UTF8String);
}


@end
