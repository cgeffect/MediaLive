#include "ffmpeg_utils.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/time.h>
}

#include "pipeline_context.h"

std::string AvErr2Str(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buf, sizeof(buf));
    return std::string(buf);
}

void PaceByTimestamp(ApiPipeline &pipeline, int64_t dts, AVRational tb) {
    if (dts == AV_NOPTS_VALUE) {
        return;
    }

    int64_t dts_us = av_rescale_q(dts, tb, AVRational{1, AV_TIME_BASE});
    if (pipeline.media_start_us == AV_NOPTS_VALUE) {
        pipeline.media_start_us = dts_us;
    }

    int64_t elapsed_us = av_gettime_relative() - pipeline.wall_start_us;
    int64_t target_us = dts_us - pipeline.media_start_us;
    if (target_us > elapsed_us) {
        av_usleep(static_cast<unsigned int>(target_us - elapsed_us));
    }
}
