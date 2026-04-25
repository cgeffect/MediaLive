#pragma once

#include <cstdint>

extern "C" {
#include <libavutil/frame.h>
}

class FrameRenderer {
public:
    void Apply(AVFrame *frame, int64_t frame_index) const;
};
