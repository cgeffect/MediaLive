#include "audio_processor.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include "ffmpeg_utils.h"

int ProcessAudioCopy(ApiPipeline &pipeline) {
    if (pipeline.audio_out_idx < 0) {
        return 0;
    }

    AVStream *ain = pipeline.in_ctx->streams[pipeline.audio_in_idx];
    AVStream *aout = pipeline.out_ctx->streams[pipeline.audio_out_idx];

    pipeline.in_pkt->stream_index = pipeline.audio_out_idx;
    av_packet_rescale_ts(pipeline.in_pkt, ain->time_base, aout->time_base);
    PaceByTimestamp(pipeline, pipeline.in_pkt->dts, aout->time_base);

    int ret = av_interleaved_write_frame(pipeline.out_ctx, pipeline.in_pkt);
    av_packet_unref(pipeline.in_pkt);
    return ret;
}
