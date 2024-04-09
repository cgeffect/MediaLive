//
//  FFAVDemuxer.hpp
//  Mogic
//
//  Created by Jason on 2022/5/31.
//  Copyright © 2022 Jason. All rights reserved.
//

#ifndef FFAVDemuxer_hpp
#define FFAVDemuxer_hpp

#include <stdio.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
namespace mogic {
class FFAVDemuxer {
public:
    FFAVDemuxer();
    ~FFAVDemuxer();
    int loadResource(const char *inPath);
    AVPacket *demuxerPacket();
    void freePacket();
    void stopDemux();
    AVStream *aStream;
    int videoindex = -1; // 视频索引
    int audioindex = -1; // 音频索引

private:
    AVFormatContext *ifmt_ctx = NULL; // 输入文件的demux
    AVCodecContext *codecContext;

    AVPacket *pkt = nullptr;
    int pkt_count = 0;
};
} // namespace mogic
#endif /* FFAVDemuxer_hpp */
