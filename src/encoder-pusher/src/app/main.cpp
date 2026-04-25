#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

extern "C" {
#include <libavutil/avutil.h>
}

#include "task_config.h"
#include "task_manager.h"

namespace {
std::atomic<bool> g_running{true};

void HandleSignal(int) {
    g_running.store(false);
}
} // namespace

int main(int argc, char **argv) {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    av_log_set_level(AV_LOG_ERROR);

    try {
        TaskConfig cfg = ParseArgs(argc, argv);

        TaskManager manager;
        if (!manager.StartTask(cfg)) {
            return 1;
        }
        manager.ListTasks();

        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        manager.StopAll();
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << std::endl;
        std::cerr << UsageText() << std::endl;
        return 1;
    }
}
