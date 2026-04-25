#pragma once

#include <atomic>

#include "frame_renderer.h"
#include "pipeline_context.h"

int ProcessVideoDecode(ApiPipeline &pipeline, const std::atomic<bool> &running, const FrameRenderer &renderer);
int FlushVideo(ApiPipeline &pipeline, const std::atomic<bool> &running);
