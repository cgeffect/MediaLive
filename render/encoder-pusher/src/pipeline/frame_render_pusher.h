#pragma once

#include <atomic>

#include "task_config.h"

class FrameRenderPusher {
public:
    explicit FrameRenderPusher(TaskConfig cfg);

    int RunFrameRenderOnce(const std::atomic<bool> &running);
    void RunCliFallback(const std::atomic<bool> &running) const;

private:
    TaskConfig cfg_;
};
