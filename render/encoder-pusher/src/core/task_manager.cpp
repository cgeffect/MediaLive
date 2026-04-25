#include "task_manager.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <utility>

#include "frame_render_pusher.h"
extern "C" {
#include <libavutil/error.h>
}

TaskManager::~TaskManager() = default;

bool TaskManager::StartTask(const TaskConfig &cfg) {
    std::lock_guard<std::mutex> lock(mu_);
    if (tasks_.count(cfg.stream_id) > 0) {
        std::cerr << "[encoder-pusher] stream already exists: " << cfg.stream_id << std::endl;
        return false;
    }

    auto task = std::make_unique<TaskState>();
    task->config = cfg;
    task->running.store(true);
    task->worker = std::thread([state = task.get()]() { RunTask(*state); });
    tasks_[cfg.stream_id] = std::move(task);
    return true;
}

void TaskManager::StopAll() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto &kv : tasks_) {
            kv.second->running.store(false);
        }
    }

    std::lock_guard<std::mutex> lock(mu_);
    for (auto &kv : tasks_) {
        if (kv.second->worker.joinable()) {
            kv.second->worker.join();
        }
    }
    tasks_.clear();
}

void TaskManager::ListTasks() const {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto &kv : tasks_) {
        const auto &cfg = kv.second->config;
        std::cout << "[encoder-pusher] streamId=" << cfg.stream_id
                  << " target=" << BuildTargetUrl(cfg)
                  << " mode=" << (cfg.input_file.empty() ? "fallback-cli-testsrc" : "frame-render-api")
                  << " input=" << (cfg.input_file.empty() ? "testsrc" : cfg.input_file)
                  << std::endl;
    }
}

void TaskManager::RunTask(TaskState &state) {
    const auto &cfg = state.config;
    std::cout << "[encoder-pusher] start streamId=" << cfg.stream_id << std::endl;

    FrameRenderPusher pusher(cfg);

    if (cfg.input_file.empty()) {
        pusher.RunCliFallback(state.running);
        std::cout << "[encoder-pusher] stop streamId=" << cfg.stream_id << std::endl;
        return;
    }

    while (state.running.load()) {
        int ret = pusher.RunFrameRenderOnce(state.running);
        if (!state.running.load()) {
            break;
        }

        if (ret == AVERROR_EOF) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        std::cerr << "[encoder-pusher] frame-render loop error streamId=" << cfg.stream_id
                  << ", err=" << ret << ", retry in 1s" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[encoder-pusher] stop streamId=" << cfg.stream_id << std::endl;
}
