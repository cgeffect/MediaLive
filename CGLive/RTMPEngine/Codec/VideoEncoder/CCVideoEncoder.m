//
//  CCVideoEncoder.m
//  001-Demo
//
//  Created by CC老师 on 2019/2/16.
//  Copyright © 2019年 CC老师. All rights reserved.
//

#import "CCVideoEncoder.h"
#import <VideoToolbox/VideoToolbox.h>

@interface CCVideoEncoder ()
//编码队列
@property (nonatomic, strong) dispatch_queue_t encodeQueue;
//回调队列
@property (nonatomic, strong) dispatch_queue_t callbackQueue;
/**编码会话*/
@property (nonatomic) VTCompressionSessionRef encodeSesion;

@end

@implementation CCVideoEncoder{
    long frameID;   //帧的递增序标识
    BOOL hasSpsPps;//判断是否已经获取到pps和sps
}

//1.初始化(配置编码参数)
- (instancetype)initWithConfig:(CCVideoConfig *)config
{
    self = [super init];
    if (self) {
        _config = config;
        _encodeQueue = dispatch_queue_create("h264 hard encode queue", DISPATCH_QUEUE_SERIAL);
        _callbackQueue = dispatch_queue_create("h264 hard encode callback queue", DISPATCH_QUEUE_SERIAL);
        
        /**编码设置*/
        
        //创建编码会话
        OSStatus status = VTCompressionSessionCreate(kCFAllocatorDefault, (int32_t)_config.width, (int32_t)_config.height, kCMVideoCodecType_H264, NULL, NULL, NULL, VideoEncodeCallback, (__bridge void * _Nullable)(self), &_encodeSesion);
        if (status != noErr) {
            NSLog(@"VTCompressionSession create failed. status=%d", (int)status);
            return self;
        }
        //设置编码器属性
        //告诉编码器这是实时编码操作（例如在实时流传输情况下，会议情况下），而不是更多的后台活动（例如转码操作）
        status = VTSessionSetProperty(_encodeSesion, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
        NSLog(@"VTSessionSetProperty: set RealTime return: %d", (int)status);
        
        //指定编码比特流的配置文件和级别。直播一般使用baseline，可减少由于b帧带来的延时
        status = VTSessionSetProperty(_encodeSesion, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_H264_Baseline_AutoLevel);
        NSLog(@"VTSessionSetProperty: set profile return: %d", (int)status);
        
        //设置码率均值(比特率可以高于此。默认比特率为零，表示视频编码器。应该确定压缩数据的大小。注意，比特率设置只在定时时有效）
        CFNumberRef bit = (__bridge CFNumberRef)@(_config.bitrate);
        status = VTSessionSetProperty(_encodeSesion, kVTCompressionPropertyKey_AverageBitRate, bit);
        NSLog(@"VTSessionSetProperty: set AverageBitRate return: %d", (int)status);
        
        //默认情况下，H.264编码器将允许对帧进行重新排序。这意味着您传递它们的演示时间戳不一定等于它们发出时的解码顺序。
        //如果要禁用此行为，您可以传递false来不允许帧重新排序。
//        status = VTSessionSetProperty(_encodeSesion, kVTCompressionPropertyKey_AllowFrameReordering, false);
//        NSLog(@"VTSessionSetProperty: set AllowFrameReordering return: %d", (int)status);

        //码率限制(只在定时时起作用)*待确认
        CFArrayRef limits = (__bridge CFArrayRef)@[@(_config.bitrate / 4), @(_config.bitrate * 4)];
        status = VTSessionSetProperty(_encodeSesion, kVTCompressionPropertyKey_DataRateLimits,limits);
        NSLog(@"VTSessionSetProperty: set DataRateLimits return: %d", (int)status);
        
        //设置关键帧间隔(GOPSize)GOP太大图像会模糊
        CFNumberRef maxKeyFrameInterval = (__bridge CFNumberRef)@(_config.fps * 2);
        status = VTSessionSetProperty(_encodeSesion, kVTCompressionPropertyKey_MaxKeyFrameInterval, maxKeyFrameInterval);
        NSLog(@"VTSessionSetProperty: set MaxKeyFrameInterval return: %d", (int)status);
        
        //设置fps(预期)
        CFNumberRef expectedFrameRate = (__bridge CFNumberRef)@(_config.fps);
        status = VTSessionSetProperty(_encodeSesion, kVTCompressionPropertyKey_ExpectedFrameRate, expectedFrameRate);
        NSLog(@"VTSessionSetProperty: set ExpectedFrameRate return: %d", (int)status);
        
        //准备编码
        status = VTCompressionSessionPrepareToEncodeFrames(_encodeSesion);
        NSLog(@"VTSessionSetProperty: set PrepareToEncodeFrames return: %d", (int)status);
    }
    return self;
}

//2.获取到sampleBuffer 数据 进行H264硬编码
- (void)encodeVideoSampleBuffer:(CMSampleBufferRef)sampleBuffer {
    CFRetain(sampleBuffer);
    dispatch_async(_encodeQueue, ^{
        //帧数据
        CVImageBufferRef imageBuffer = (CVImageBufferRef)CMSampleBufferGetImageBuffer(sampleBuffer);
        //该帧的时间戳
        self->frameID++;
        CMTime timeStamp = CMTimeMake(self->frameID, 1000);
        //持续时间
        CMTime duration = kCMTimeInvalid;
        //编码
        VTEncodeInfoFlags flags;
        OSStatus status = VTCompressionSessionEncodeFrame(self.encodeSesion, imageBuffer, timeStamp, duration, NULL, NULL, &flags);
        if (status != noErr) {
            NSLog(@"VTCompression: encode failed: status=%d",(int)status);
        }
        CFRelease(sampleBuffer);
    });
    
}


// startCode 长度 4
const Byte startCode[] = "\x00\x00\x00\x01";
//编码成功回调
void VideoEncodeCallback(void * CM_NULLABLE outputCallbackRefCon, void * CM_NULLABLE sourceFrameRefCon,OSStatus status, VTEncodeInfoFlags infoFlags,  CMSampleBufferRef sampleBuffer ) {
    
    if (status != noErr) {
        NSLog(@"VideoEncodeCallback: encode error, status = %d", (int)status);
        return;
    }
    if (!CMSampleBufferDataIsReady(sampleBuffer)) {
        NSLog(@"VideoEncodeCallback: data is not ready");
        return;
    }
    CCVideoEncoder *encoder = (__bridge CCVideoEncoder *)(outputCallbackRefCon);
    
    //判断是否为关键帧
    BOOL keyFrame = NO;
    CFArrayRef attachArray = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    //(注意取反符号)
    keyFrame = !CFDictionaryContainsKey(CFArrayGetValueAtIndex(attachArray, 0), kCMSampleAttachmentKey_NotSync);
    //VideoToolbox编码器在每一个关键帧 前面都会输出SPS和PPS信息，所以如果本帧是关键帧，则取出对应的 SPS和PPS信息。
    //获取sps & pps 数据 ，只需获取一次，保存在h264文件开头即可
    if (keyFrame && !encoder->hasSpsPps) {
        size_t spsSize, spsCount;
        size_t ppsSize, ppsCount;
        const uint8_t *spsData, *ppsData;
        //获取图像源格式
        CMFormatDescriptionRef formatDesc = CMSampleBufferGetFormatDescription(sampleBuffer);
        OSStatus status1 = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(formatDesc, 0, &spsData, &spsSize, &spsCount, 0);
        OSStatus status2 = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(formatDesc, 1, &ppsData, &ppsSize, &ppsCount, 0);
        
        //判断sps/pps获取成功
        if (status1 == noErr & status2 == noErr) {
            NSLog(@"VideoEncodeCallback： get sps, pps success");
            encoder->hasSpsPps = true;
            //sps data
            NSMutableData *sps = [NSMutableData dataWithCapacity:4 + spsSize];
            [sps appendBytes:startCode length:4];
            [sps appendBytes:spsData length:spsSize];
            //pps data
            NSMutableData *pps = [NSMutableData dataWithCapacity:4 + ppsSize];
            [pps appendBytes:startCode length:4];
            [pps appendBytes:ppsData length:ppsSize];
            
            dispatch_async(encoder.callbackQueue, ^{
                //回调方法传递sps/pps
                [encoder.delegate videoEncodeCallbacksps:sps pps:pps];
            });
            
        } else {
            NSLog(@"VideoEncodeCallback： get sps/pps failed spsStatus=%d, ppsStatus=%d", (int)status1, (int)status2);
        }
    }
    
    /// 获取NALU数据
    size_t lengthAtOffset, totalLength;
    char *dataPoint;
    
    //将数据复制到dataPoint
    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    OSStatus error = CMBlockBufferGetDataPointer(blockBuffer, 0, &lengthAtOffset, &totalLength, &dataPoint);
    if (error != kCMBlockBufferNoErr) {
        NSLog(@"VideoEncodeCallback: get datapoint failed, status = %d", (int)error);
        return;
    }
    //https://blog.csdn.net/qq_24384939/article/details/88764744
    /*
     SPS            （4字节头）
     PPS            （4字节头）
     SEI            （4字节头）
     I0(slice0)     （4字节头）
     I0(slice1)    （3字节头）
     P1(slice0)     （4字节头）
     P1(slice1)    （3字节头）
     P2(slice0)     （4字节头）
     P2(slice1)    （3字节头）
     */
    //循环获取nalu数据
    size_t offet = 0;
    /**
     * h264有两种封装
     * 一种是annexb模式，传统模式，有startcode（0x000001或0x0000001）sps, pps 在ES层(S流(Elementary Stream): 也叫基本码流,包含视频、音频或数据的 连续码流.)
     * 一种是MP4模式，一般用mp4、mkv、flv容器封装 都是mp4模式，没有startcode, sps和pps以及其它信息被封装在container中, 每一个frame的前四个字节是这个frame的长度
     */
    //返回的nalu数据前四个字节不是0001的startcode(不是系统端的0001)，而是大端模式的帧长度length(MP4模式), VideoToolbox编码出来的数据是MP4模式,每一个frame的前四个字节是这个frame的长度
    const int lengthInfoSize = 4;
    /**
     * 一般情况下,每一次编码回调都是出来一个nalu数据, 只有第一帧的时候, 会出来一个sps,pps,sei, I帧, 所以第一次回调的数据里包含多个nalu,
     * 采用while循环获取每一个nalu
     * sps, pps在上面获取, 这里只获取sei和I帧
     */
    while (offet < totalLength - lengthInfoSize) {
        uint32_t naluLength = 0;
        //获取nalu 数据长度, 获取每一个nalu的前四个字节
        memcpy(&naluLength, dataPoint + offet, lengthInfoSize);
        //大端转系统端(小端模式), 即可获取nalu的值, 该值即为当前nalu的长度
        naluLength = CFSwapInt32BigToHost(naluLength);
        //获取到编码好的视频数据
        NSMutableData *data = [NSMutableData dataWithCapacity:4 + naluLength];
        //首先在nalu前面拼接startCode
        [data appendBytes:startCode length:4];
        //再拼接其他数据
        [data appendBytes:dataPoint + offet + lengthInfoSize length:naluLength];
        uint8_t *nalu = (uint8_t *)data.bytes;
        int type = (nalu[4] & 0x1F);
        
//        switch (type) {
//            case 0x05: //关键帧
//                NSLog(@"type = %d I帧", type);
//                break;
//            case 0x06://增强信息
//                NSLog(@"type = %d SEI", type);
//                break;
//            case 0x07: //sps
//                NSLog(@"type = %d sps", type);
//                break;
//            case 0x08: //pps
//                NSLog(@"type = %d pps", type);
//                break;
//            default: //其他帧（1-5）
//                NSLog(@"type = %d 其他帧", type);
//                break;
//        }
        //将NALU数据回调到代理中
        dispatch_async(encoder.callbackQueue, ^{
            [encoder.delegate videoEncodeCallback:data isKeyFrame:keyFrame];
        });
        
        //移动下标，继续读取下一个nalu数据
        offet += lengthInfoSize + naluLength;
    }
  
}

- (void)dealloc
{
    if (_encodeSesion) {
        VTCompressionSessionCompleteFrames(_encodeSesion, kCMTimeInvalid);
        VTCompressionSessionInvalidate(_encodeSesion);
        
        CFRelease(_encodeSesion);
        _encodeSesion = NULL;
    }
    
}



@end
