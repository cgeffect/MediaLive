//
//  DecodeMM.h
//  MediaLive
//
//  Created by Jason on 2024/4/9.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface DecodeMM : NSObject

- (void)video_filter_decode:(NSString *)in0 in1:(NSString *)in1 ou:(NSString *)ou;

- (void)video_filter:(NSString *)in0 ou:(NSString *)ou;

@end

NS_ASSUME_NONNULL_END
