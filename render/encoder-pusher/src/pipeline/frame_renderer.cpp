#include "frame_renderer.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

void FrameRenderer::Apply(AVFrame *frame, int64_t frame_index) const {
    if (frame == nullptr || frame->format != AV_PIX_FMT_YUV420P) {
        return;
    }

    const int width = frame->width;
    const int height = frame->height;
    const int line = frame->linesize[0];

    int band_start = static_cast<int>((frame_index * 4) % (width + 80)) - 80;
    int band_end = band_start + 80;

    for (int y = 0; y < height; ++y) {
        uint8_t *row = frame->data[0] + y * line;
        for (int x = 0; x < width; ++x) {
            if (x >= band_start && x < band_end) {
                int v = row[x] + 28;
                row[x] = static_cast<uint8_t>(v > 255 ? 255 : v);
            }
        }
    }
}
