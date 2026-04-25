#pragma once

#include <cstdint>
#include <string>

extern "C" {
#include <libavutil/rational.h>
}

struct ApiPipeline;

std::string AvErr2Str(int err);
void PaceByTimestamp(ApiPipeline &pipeline, int64_t dts, AVRational tb);
