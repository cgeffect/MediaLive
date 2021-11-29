//
//  AVVideoDemuxer_hpp
//  FFmpegiOS
//
//  Created by Jason on 2020/11/11.
//  Copyright © 2020 Jason. All rights reserved.
//

#ifndef AVVideoDemuxer_hpp
#define AVVideoDemuxer_hpp

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

/*
 解封装, 从本地文件读取视频流h264和音频流aac
 */
class AVVideoDemuxer {
public:
    int ffmpeg_demux_(const char *path);
    AVFormatContext *fmt_ctx;
    int videoStreamIndex;
private:
    void demux_failed(AVFormatContext *ifmt_ctx);
};
#endif /* AVVideoDemuxer_hpp */
