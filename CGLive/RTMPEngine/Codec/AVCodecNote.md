#  编解码
1. CMSampleBuffer可以包装包含压缩的音频或视频数据的CMBlockBuffer。其次，CMSampleBuffer可以包装包含未压缩栅格图像数据的CVPixelBuffer。如您所见，这两种类型的CMSampleBuffers都包含CMTime值，这些值描述了样本表示和解码时间戳。它们还包含CMFormatDesc，该格式携带描述样本缓冲区中数据格式的信息。
2. CMSampleBuffers还可以携带附件，这将我们带到了CMSampleBuffer的第三种类型，即标记CMSampleBuffer，它没有CMBlockBuffer或CVPixelBuffer，并且完全存在以通过定时通知特定条件的媒体管道携带定时附件。
3. 启用硬解码
4. 禁用硬解码
5. 解码器如何在单独的进程中运行
6. 自定义IOSurface
```
https://zhuanlan.zhihu.com/p/24762605
+(CVPixelBufferRef)createPixelBufferWithSize:(CGSize)size {
    const void *keys[] = {
        kCVPixelBufferOpenGLESCompatibilityKey,
        kCVPixelBufferIOSurfacePropertiesKey,
    };
    const void *values[] = {
        (__bridge const void *)([NSNumber numberWithBool:YES]),
        (__bridge const void *)([NSDictionary dictionary])
    };
    
    OSType bufferPixelFormat = kCVPixelFormatType_32BGRA;
    
    CFDictionaryRef optionsDictionary = CFDictionaryCreate(NULL, keys, values, 2, NULL, NULL);
    
    CVPixelBufferRef pixelBuffer = NULL;
    CVPixelBufferCreate(kCFAllocatorDefault,
                        size.width,
                        size.height,
                        bufferPixelFormat,
                        optionsDictionary,
                        &pixelBuffer);
    
    CFRelease(optionsDictionary);
    
    return pixelBuffer;
}
```

AVAssetReader三个选项
1. 提供压缩的数据而无需先对其进行解码, 为了从AVAssetReader获得原始压缩输出，您只需要按照前面所述构造AVAsset Reader。但是，在创建AVAssetReaderTrackOutput时，您会将输出设置设置为nil，而不是提供指定像素格式的字典。
```
AVAssetReaderTrackOutput* assetReaderTrackOutput
= [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:track
                                             outputSettings:nil];
```

AVSampleBufferGenerator 提供直接从AVAsset轨道中的媒体读取的样本。
为了使AVSampleBufferGenerator达到最佳性能，您需要提供一个时基并异步运行请求。您可以使用源AVAsset创建AVSampleBufferGenerator。
```
//首先，您需要创建一个AVSampleCursor，它将用于逐步浏览示例。
AVSampleCursor* cursor = [assetTrack makeSampleCursorAtFirstSampleInDecodeOrder];
//您还必须创建一个AVSampleBufferRequest，它描述将要发出的实际样本请求。
AVSampleBufferRequest* request = [[AVSampleBufferRequest alloc] initWithStartCursor:cursor];
        
request.direction = AVSampleBufferRequestDirectionForward;
request.preferredMinSampleCount = 1;
request.maxSampleCount = 1;
//现在，您可以使用源AVAsset创建AVSampleBufferGenerator。
//请注意，我在此处将时间设置为nil，这将导致同步操作。
AVSampleBufferGenerator* generator
= [[AVSampleBufferGenerator alloc] initWithAsset:srcAsset timebase:nil];

BOOL notDone = YES;
    
while(notDone)
{
//最后，我遍历对createSampleBufferForRequest的调用，并将光标一次向前移一帧。同样，这显示了最简单的同步操作。为了获得最佳性能，可以使用这些请求的异步版本。
   CMSampleBufferRef sampleBuffer = [generator createSampleBufferForRequest:request];

   // do your thing with the sampleBuffer

   [cursor stepInDecodeOrderByCount:1];
}
```
打包CMSampleBuffer 到 CMBlockBuffer
首先需要将示例数据打包在CMBlockBuffer中。然后，您需要创建一个描述数据的CMVideoFormatDescription。在这里，将颜色标签包括在扩展词典中非常重要，以确保对视频进行正确的颜色管理。接下来，您将在CMSampleTimingInfo结构中创建一些时间戳。最后，使用CMBlockBuffer，CMVideoFormatDescription和CMSampleTimingInfo创建CMSampleBuffer。好的，您已经决定要自己做，并且已经在视频工具箱中为CMSampleBuffers创建了源。
```
CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, sampleData, sizeof(sampleData), 
                                   kCFAllocatorMalloc, NULL, 0, sizeof(sampleData), 0, 
                                   &blockBuffer);

CMVideoFormatDescriptionCreate(kCFAllocatorDefault, kCMVideoCodecType_AppleProRes4444, 1920, 
                               1080, extensionsDictionary, &formatDescription);

CMSampleTimingInfo timingInfo;

timingInfo.duration = CMTimeMake(10, 600);
timingInfo.presentationTimeStamp = CMTimeMake(frameNumber * 10, 600);

CMSampleBufferCreateReady(kCFAllocatorDefault, blockBuffer, formatDescription, 1, 1, 
                          &timingInfo, 1, &sampleSize, &sampleBuffer);
```


在当前的OS版本上，默认情况下会为支持它的所有格式启用硬件解码器的使用。在当前的操作系统中，默认情况下所有硬件加速编解码器均可用，而无需选择加入。
## 创建VTDCompressionSession的过程。需要指定三个主要选项
### 首先是视频格式说明。
这告诉VTDecompressionSession将使用哪种编解码器，并提供有关CMSampleBuffers中数据格式的更多详细信息。这应该与您要发送到会话的CMSampleBuffers的CMVideoFormatDescription相匹配
### 接下来是destinationImageBufferAttributes
它描述您的输出pixelBuffer的要求。如果希望“视频工具箱”将输出缩放到特定大小，则可以包括尺寸。如果渲染引擎需要，则它可以包含特定的像素格式。如果您只知道如何使用8位RGB样本，则可以在此处进行请求。
### 接下来是videoDecoderSpecification
它提供有关解码器选择因素的提示。您可以在此处指定非默认硬件解码器请求。

如果要保证使用硬件解码器创建VTDCompressionSession并希望在不可能的情况下失败创建会话，则可以将RequiredHardwareAcceleratedVideoDecoder规范选项设置为true。同样，如果要禁用硬件解码并使用软件解码器，则可以包括设置为false的EnableHardwareAcceleratedVideoDecoder规范选项。这两个键非常相似。因此，第一个键是RequireHardwareAcceleratedVideoDecoder，第二个键是EnableHardwareAcceleratedVideoDecoder。此示例显示了VTDecompressionSession创建的基础。我们需要的第一件事是格式描述，以告诉会话期望什么类型的数据。我们直接从CMSampleBuffer中提取该值，该值稍后将传递给会话。如果我们希望输出使用特定的像素格式，则需要创建一个pixelBufferAttributes字典来描述所需内容。因此，就像前面的AVAssetReader示例一样，我们要求带alpha的16位4444 YcbCr。现在，我们可以创建VTDecompressionSession。


###  VTDecompressionSession Creation
```

CMFormatDescriptionRef formatDesc = CMSampleBufferGetFormatDescription(sampleBuffer);

CFDictionaryRef pixelBufferAttributes = (__bridge CFDictionaryRef)@{
    (id)kCVPixelBufferPixelFormatTypeKey :
    @(kCVPixelFormatType_4444AYpCbCr16) };

VTDecompressionSessionRef decompressionSession;
    
OSStatus err = VTDecompressionSessionCreate(kCFAllocatorDefault, 
                                            formatDesc, 
                                            NULL, //请注意，我们为第三个参数（视频解码器规范）输入了null。此null表示视频工具箱将执行其默认硬件解码器选择。
                                            pixelBufferAttributes, 
                                            NULL, 
                                            &decompressionSession);
```

### Running a VTDecompressionSession
```
uint32_t inFlags = kVTDecodeFrame_EnableAsynchronousDecompression;

VTDecompressionOutputHandler  outputHandler
 = ^(OSStatus status,
     VTDecodeInfoFlags infoFlags,
     CVImageBufferRef imageBuffer,
     CMTime presentationTimeStamp,
     CMTime presentationDurationVTDecodeInfoFlags)
 {
     // Handle decoder output in this block
     // Status reports any decoder errors
     // imageBuffer contains the decoded frame if there were no errors
 };

VTDecodeInfoFlags outFlags;

OSStatus err = VTDecompressionSessionDecodeFrameWithOutputHandler(decompressionSession,
                                                   sampleBuffer, inFlags, 
                                                   &outFlags, outputHandler);
```

https://developer.apple.com/videos/play/wwdc2020/10090/
