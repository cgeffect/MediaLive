//
//  LiveVideoPublisher.cpp
//  FFmpegiOS
//
//  Created by Jason on 2021/4/13.
//  Copyright © 2021 Jason. All rights reserved.
//

#include "LiveVideoPublisher.h"
#include "LivePlatformCommon.h"
#include <libavutil/opt.h>

#define is_start_code(code) (((code) & 0x0ffffff) == 0x01)
int get_sr_index(unsigned int sampling_frequency);
LiveVideoPublisher::LiveVideoPublisher() {
    
}

LiveVideoPublisher::~LiveVideoPublisher() {
    
}

double LiveVideoPublisher::getVideoStreamTimeInSecs() {
    return lastPresentationTimeMs / 1000.0f;
}
double LiveVideoPublisher::getAudioStreamTimeInSecs() {
    return lastAudioPacketPresentationTimeMills / 1000.0f;
}

/* avio_open2函数的返回值大于等于0，则将 isConnected变量设置为true，代表其已经成功地打开了文件输出通道。 唯一需要注意的一点是，需要配置一个超时回调函数进去，这个回调函 数主要是给FFmpeg的协议层用的，在实现这个函数的时候，返回1则代 表结束I/O操作，返回0则代表继续I/O操作 */
int LiveVideoPublisher::interrupt_cb(void *ctx) { // 超时回调函数
    LiveVideoPublisher *publisher = (LiveVideoPublisher *)ctx;
    long diff = platform_4_live::getCurrentTimeMills() - publisher->latestFrameTime;
    if (diff > publisher->publishTimeout) {
//        int queueSize = LivePacketPool::GetInstance()->getRecordingVideoPacketQueueSize();
        printf("LiveVideoPublisher::interrupt_cb callback time out ... queue size:%ld\n", diff);
        return 1; // 返回 1 则代表结束 I/O 操作
    }
    return 0;
}

int LiveVideoPublisher::init(char *videoOutputURI, int videoWidth, int videoHeight, float videoFrameRate, int videoBitRate, int audioSampleRate, int audioChannels, int audioBitRate, char *audio_codec_name) {
    latestFrameTime = platform_4_live::getCurrentTimeMills();
//    avcodec_register_all();
//    av_register_all();
    av_log_set_level(AV_LOG_DEBUG);
    avformat_network_init();

    printf("Publish URL %s\n", videoOutputURI);

    avformat_alloc_output_context2(&oc, NULL, "flv", (const char *)videoOutputURI);
    if (!oc) {
        return -1;
    }
    fmt = oc->oformat;

    //构造视频流
    video_st = buildVideoStream(oc, videoWidth, videoHeight, videoFrameRate, videoBitRate);
    
    //构造音频流
    audio_st = buildAudioStream(oc, audioSampleRate, audioChannels, audioBitRate, audio_codec_name);
    
    //打开连接通道
    if (!(fmt->flags & AVFMT_NOFILE)) {
        AVIOInterruptCB int_cb = { interrupt_cb, this};
        oc->interrupt_callback = int_cb;
        int ret = avio_open2(&oc->pb, videoOutputURI, AVIO_FLAG_WRITE, &oc->interrupt_callback, NULL);
        if (ret < 0) {
            printf("Could not open '%s': %s\n", videoOutputURI, av_err2str(ret));
            return -1;
        }
        this->isConnected = true;
    }
    return 1;
}

AVStream* LiveVideoPublisher::buildVideoStream(AVFormatContext *oc, int videoWidth, int videoHeight, float videoFrameRate, int videoBitRate) {
    fmt->video_codec = AV_CODEC_ID_H264;
    //找出H264的编码器
    AVCodec *video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (video_codec == NULL) {
        printf("Could not find encoder for '%s'\n", avcodec_get_name(AV_CODEC_ID_H264));
        return NULL;
    }
    printf("\n find encoder name is '%s'\n", video_codec->name);

    //增加h264流
    AVStream *st = avformat_new_stream(oc, video_codec);
    st->id = oc->nb_streams - 1;
    //流的编码器上下文
    AVCodecContext *c = st->codec;
    c->codec_id = AV_CODEC_ID_H264;
    c->bit_rate = videoBitRate;
    c->width = videoWidth;
    c->height = videoHeight;
    
    st->avg_frame_rate.num = 30000;
    st->avg_frame_rate.den = (int) (30000 / videoFrameRate);
    
    c->time_base.den = 30000;
    c->time_base.num = (int) (30000 / videoFrameRate);
    c->gop_size = videoFrameRate; //表示两个I帧之间的间隔
    
    c->qmin = 10;
    c->qmax = 30;
    c->pix_fmt = AV_PIX_FMT_BGRA;
    // 新增语句，设置为编码延迟
    av_opt_set(c->priv_data, "preset", "ultrafast", 0);
    // 实时编码关键看这句，上面那条无所谓
    av_opt_set(c->priv_data, "tune", "zerolatency", 0);
    
    printf("sample_aspect_ratio = %d   %d", c->sample_aspect_ratio.den, c->sample_aspect_ratio.num);
    
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
       c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    return st;
}

AVStream* LiveVideoPublisher::buildAudioStream(AVFormatContext *oc, int audioSampleRate, int audioChannels, int audioBitRate, char *audio_codec_name) {
    printf("audioBitRate is %d audioChannels is %d audioSampleRate is %d\n", audioBitRate,
         audioChannels, audioSampleRate);
    fmt->audio_codec = AV_CODEC_ID_AAC;
    AVCodec *audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVStream *st = avformat_new_stream(oc, audio_codec);
    st->id = oc->nb_streams - 1;
    AVCodecContext *c = st->codec;
    c->sample_fmt = AV_SAMPLE_FMT_S16;
    c->bit_rate = audioBitRate;
    c->codec_type = AVMEDIA_TYPE_AUDIO;
    c->sample_rate = audioSampleRate;
    c->channel_layout = audioChannels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    /*
     和视频流不同的是，完成音频流的添加之后，这里需要为编码器上 下文设置一下extradata变量，FFmpeg对于在编码端设置这个变量的目的 是，为解码器提供原始数据，从而初始化解码器。还记得之前编码AAC 的时候要在编码出来的数据前面加上ADTS的头吗?其实在ADTS头部 信息里面可以提取出编码器的Profile、采样率以及声道数的信息，但是 在FFmpeg中并不是每一个AAC的头部都能添加上这些信息，而是在全 局的extradata中配置这些信息。
     */
    c->extradata = (uint8_t *)av_malloc(2);
    c->extradata_size = 2;
    int profile = 2;  // AAC LC
    int freqIdx = 4;  // 44.1Khz
    int chanCfg = 2;  // Stereo Channel

    char dsi[2];
    dsi[0] = (profile << 3) | (freqIdx >> 1);
    dsi[1] = (freqIdx & 1 << 7) | (chanCfg << 3);
    memcpy(c->extradata, dsi, 2); // FFmpeg 设置 extradata 的目的是为解码器提供原始数据，从而初始化解码器，类似于编码 AAC 前面加上 ADTS 的头，ADTS 头部信息可以提取编码器的 Profile、采样率以及声道数的信息
    // This filter creates an MPEG-4 AudioSpecificConfig from an MPEG-2/4 ADTS header and removes the ADTS header.
    bsfc = av_bitstream_filter_init("aac_adtstoasc");
    
//    int profile = 2;  // AAC LC
//    int freqIdx = 4;  // 44.1Khz
//    int chanCfg = 2;  // Stereo Channel
//    char dsi[2];
//    dsi[0] = (profile << 3) | (freqIdx >> 1);
//    dsi[1] = ((freqIdx & 1) << 7) | (chanCfg << 3);
//    memcpy(c->extradata, dsi, 2);
//    //音频格式转 换的滤波器，就是ADTS到ASC格式的转换器
//    bsfc = av_bitstream_filter_init("aac_adtstoasc");
    
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    return st;
}

int LiveVideoPublisher::pushVideoPacket(LiveVideoPacket *packet) {
    if (!isConnected) {
        printf("not connect\n");
        return 0;
    }
    if (index % 2 == 0) {
        return 0;
    }
    printf("index video = %d", index);
    int ret = 0;
    double video_time = getVideoStreamTimeInSecs();
    double audio_time = getAudioStreamTimeInSecs();
//    if (audio_time < video_time){
//      ret = write_audio_frame(oc, audio_st, NULL);
//    } else {
//      ret = write_video_frame(oc, video_st, packet);
//    }
    ret = write_video_frame(oc, video_st, packet);
    latestFrameTime = platform_4_live::getCurrentTimeMills();
//    duration = MIN(audio_time, video_time);
    duration = video_time;
    index++;
    return ret;
}

int LiveVideoPublisher::write_video_frame(AVFormatContext *oc, AVStream *st, LiveVideoPacket *packet) {
    int ret = 0;
    LiveVideoPacket *h264Packet = packet;
    videoStreamTimeInSecs = h264Packet->timeMills / 1000.0;
    AVCodecContext *c = st->codec;
    lastPresentationTimeMs = h264Packet->timeMills;
    int bufferSize = (h264Packet)->size;
    uint8_t* outputData = (uint8_t *) ((h264Packet)->buffer);
        
    int nalu_type = (outputData[4] & 0x1F);
    //如果是sps/pps
    if (nalu_type == H264_NALU_TYPE_SEQUENCE_PARAMETER_SET) {
        // 我们这里要求 sps 和 pps 一块拼接起来构造成 AVPacket 传过来, 通过分割startCode
        int headerSize = bufferSize;
        uint8_t *headerData = new uint8_t[headerSize];
        memcpy(headerData, outputData, bufferSize);
        
        //填充extradata
        uint8_t *spsFrame = 0;
        uint8_t *ppsFrame = 0;
        int spsFrameLen = 0;
        int ppsFrameLen = 0;
        
        //这里需要解析sps/pps
        parseH264SequenceHeader(headerData, headerSize, &spsFrame, spsFrameLen, &ppsFrame, ppsFrameLen);

        uint8_t *nalu2 = (uint8_t *)ppsFrame;
        for (int i = 0; i < ppsFrameLen; i++) {
            printf("%02x ", nalu2[i]);
        }
        printf("\n");
        
        uint8_t *nalu = (uint8_t *)spsFrame;
        for (int i = 0; i < spsFrameLen; i++) {
            printf("%02x ", nalu[i]);
        }
        printf("\n");
        
        // 将 SPS 和 PPS 封装到视频编码器上下文的 extradata 中，参考 FFmpeg 源码中 avc.c
        int extradata_len = 8 + spsFrameLen - 4 + 1 + 2 + ppsFrameLen - 4;
        c->extradata = (uint8_t *)av_mallocz(extradata_len);
        c->extradata_size = extradata_len;
        c->extradata[0] = 0x01; // version
        c->extradata[1] = spsFrame[4 + 1];  // profile
        c->extradata[2] = spsFrame[4 + 2];  // profile compat
        c->extradata[3] = spsFrame[4 + 3];  // level
        c->extradata[4] = 0xFC | 3; // 保留位
        c->extradata[5] = 0xE0 | 1; // 保留位
        int tmp = spsFrameLen - 4; // 开始写 SPS
        c->extradata[6] = (tmp >> 8) & 0x00ff;
        c->extradata[7] = tmp & 0x00ff;
        int i = 0;
        for (i = 0; i < tmp; i++) {
            c->extradata[8 + i] = spsFrame[4 + i];
        }
        c->extradata[8 + tmp] = 0x01; // 结束写 SPS
        int tmp2 = ppsFrameLen - 4; // 开始写 PPS
        c->extradata[8 + tmp + 1] = (tmp2 >> 8) & 0x00ff;
        c->extradata[8 + tmp + 2] = tmp2 & 0x00ff;
        for (i = 0; i < tmp2; i++) {
            c->extradata[8 + tmp + 3 + i] = ppsFrame[4 + i];
        }
        // 结束写 PPS
        int ret = avformat_write_header(oc, NULL);
        if (ret < 0) {
            printf("Error occurred when opening output file: %s\n", av_err2str(ret));
        } else {
            isWriteHeaderSuccess = true;
        }
    } else {
        int64_t cal_pts = lastPresentationTimeMs / 1000.0f / av_q2d(video_st->time_base);
        int64_t pts = h264Packet->pts == PTS_PARAM_UN_SETTIED_FLAG ? cal_pts : h264Packet->pts;
        int64_t dts = h264Packet->dts == DTS_PARAM_UN_SETTIED_FLAG ? pts : h264Packet->dts == DTS_PARAM_NOT_A_NUM_FLAG ? AV_NOPTS_VALUE : h264Packet->dts;
        printf("pts %lld, dts %lld\n", pts, dts);
        //H264数据封装成FFmpeg认识的 AVPacket,把 H264视频帧起始的StartCode部分替换为这一帧视频帧的大小即可，当 然大小不包括这个StartCode部分
        AVPacket *pkt = av_packet_alloc();
        pkt->stream_index = st->index;
        pkt->data = outputData;
        pkt->size = bufferSize;
        if(pkt->data[0] == 0x00 && pkt->data[1] == 0x00 &&
                pkt->data[2] == 0x00 && pkt->data[3] == 0x01){
            bufferSize -= 4;
            pkt->data[0] = ((bufferSize) >> 24) & 0x00ff;
            pkt->data[1] = ((bufferSize) >> 16) & 0x00ff;
            pkt->data[2] = ((bufferSize) >> 8) & 0x00ff;
            pkt->data[3] = ((bufferSize)) & 0x00ff;
        }
        pkt->pts = pts;
        pkt->dts = dts;
        if (nalu_type == H264_NALU_TYPE_IDR_PICTURE || nalu_type == H264_NALU_TYPE_SEI) {
            pkt->flags = AV_PKT_FLAG_KEY; // 标识为关键帧
        } else {
            pkt->flags = 0;
        }
        c->frame_number++;
        
        //输出
        int ret = av_interleaved_write_frame(oc, pkt);
        if (ret < 0) {
            printf("error = %d\n", AVERROR(ret));
           
        }
        av_packet_free(&pkt);
    }
    return ret;
}

int LiveVideoPublisher::pushAudioPacket(LiveAudioPacket *packet) {
    if (!isConnected) {
        printf("not connect\n");
        return 0;
    }
    if (index % 2 == 1) {
        return 0;
    }
    printf("index audio = %d", index);

    int ret = write_audio_frame(oc, audio_st, packet);
    latestFrameTime = platform_4_live::getCurrentTimeMills();
    double video_time = getVideoStreamTimeInSecs();
    duration = video_time;
    index++;
    return ret;
}

int LiveVideoPublisher::write_audio_frame(AVFormatContext *oc, AVStream *st, LiveAudioPacket *packet) {
    LiveAudioPacket *audioPacket = packet;
    lastAudioPacketPresentationTimeMills = packet->position;
    
    //AVPacket
    AVPacket pkt = { 0 };
    av_init_packet(&pkt);
    pkt.data = audioPacket->data;
    pkt.size = audioPacket->size;
    pkt.dts = pkt.pts = lastAudioPacketPresentationTimeMills / 1000.0f /
           av_q2d(st->time_base);
    pkt.duration = 1024;
    pkt.stream_index = st->index;
    
    //将ADTS封装格式的AAC变成了一个 MPEG4封装格式的AAC
    AVPacket newpacket;
    av_init_packet(&newpacket);
    int ret = av_bitstream_filter_filter(bsfc, st->codec, NULL, &newpacket.data, &newpacket.size, pkt.data, pkt.size, pkt.flags & AV_PKT_FLAG_KEY);
    if (ret >= 0) {
        newpacket.pts = pkt.pts;
        newpacket.dts = pkt.dts;
        newpacket.duration = pkt.duration;
        newpacket.stream_index = pkt.stream_index;
        ret = av_interleaved_write_frame(oc, &newpacket);
    }
    av_free_packet(&newpacket);
    av_free_packet(&pkt);
//    delete audioPacket;
    return 0;
}

void LiveVideoPublisher::stop() {
    //如果我们没有write header而又在销毁的时候 调用了write trailer，那么FFmpeg程序会直接崩溃
    av_write_trailer(oc);
    //关闭音频bitStreamFilter
    av_bitstream_filter_close(bsfc);
    //关闭输出通道
    avio_close(oc->pb);
    //释放整个AVFormatContext
    avformat_free_context(oc);

}

void LiveVideoPublisher::parseH264SequenceHeader(uint8_t *in_pBuffer, uint32_t in_ui32Size, uint8_t **inout_pBufferSPS, int &inout_ui32sizeSPS, uint8_t **inout_pBufferPPS, int &inout_ui32sizePPS) {
    uint32_t ui32StartCode = 0x0ff;
    
    uint8_t *pBuffer = in_pBuffer;
    uint32_t ui32BufferSize = in_ui32Size;
    
    uint32_t sps = 0;
    uint32_t pps = 0;
    
    uint32_t idr = in_ui32Size;
    
    do {
        uint32_t ui32ProcessedBytes = 0;
        ui32StartCode = findStartCode(pBuffer, ui32BufferSize, ui32StartCode, ui32ProcessedBytes);
        pBuffer += ui32ProcessedBytes;
        ui32BufferSize -= ui32ProcessedBytes;
        
        if (ui32BufferSize < 1) {
            break;
        }
        
        uint8_t val = (*pBuffer & 0x1f);
        
        if (val == 5) {
            idr = pps + ui32ProcessedBytes - 4;
        }
        if (val == 7) {
            sps = ui32ProcessedBytes;
        }
        if (val == 8) {
            pps = sps + ui32ProcessedBytes;
        }
    } while (ui32BufferSize > 0);
    
    *inout_pBufferSPS = in_pBuffer + sps - 4;
    inout_ui32sizeSPS = pps - sps;
    
    *inout_pBufferPPS = in_pBuffer + pps - 4;
    inout_ui32sizePPS = idr - pps + 4;
}

uint32_t LiveVideoPublisher::findStartCode(uint8_t *in_pBuffer, uint32_t in_ui32BufferSize, uint32_t in_ui32Code, uint32_t &out_ui32ProcessedBytes) {
    uint32_t ui32Code = in_ui32Code;
    
    const uint8_t *ptr = in_pBuffer;
    while (ptr < in_pBuffer + in_ui32BufferSize) {
        ui32Code = *ptr++ + (ui32Code << 8);
        if (is_start_code(ui32Code)) {
            break;
        }
    }
    
    out_ui32ProcessedBytes = (uint32_t)(ptr - in_pBuffer);
    return ui32Code;
}


int get_sr_index(unsigned int sampling_frequency) {
    switch (sampling_frequency) {
        case 96000:
            return 0;
        case 88200:
            return 1;
        case 64000:
            return 2;
        case 48000:
            return 3;
        case 44100:
            return 4;
        case 32000:
            return 5;
        case 24000:
            return 6;
        case 22050:
            return 7;
        case 16000:
            return 8;
        case 12000:
            return 9;
        case 11025:
            return 10;
        case 8000:
            return 11;
        case 7350:
            return 12;
        default:
            return 0;
    }
}
