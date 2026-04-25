#include "task_config.h"

#include <stdexcept>

std::string BuildTargetUrl(const TaskConfig &cfg) {
    return "rtmp://" + cfg.host + ":" + std::to_string(cfg.port) + "/" + cfg.app + "/" + cfg.stream_id;
}

TaskConfig ParseArgs(int argc, char **argv) {
    TaskConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto get_val = [&](const char *name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };

        if (arg == "--stream-id") {
            cfg.stream_id = get_val("--stream-id");
        } else if (arg == "--host") {
            cfg.host = get_val("--host");
        } else if (arg == "--port") {
            cfg.port = std::stoi(get_val("--port"));
        } else if (arg == "--app") {
            cfg.app = get_val("--app");
        } else if (arg == "--input") {
            cfg.input_file = get_val("--input");
        } else if (arg == "--fps") {
            cfg.fps = std::stoi(get_val("--fps"));
        } else if (arg == "--width") {
            cfg.width = std::stoi(get_val("--width"));
        } else if (arg == "--height") {
            cfg.height = std::stoi(get_val("--height"));
        } else if (arg == "--audio-freq") {
            cfg.audio_freq = std::stoi(get_val("--audio-freq"));
        } else {
            throw std::runtime_error("unknown arg: " + arg);
        }
    }

    return cfg;
}

std::string UsageText() {
    return "Usage: encoder_pusher [--stream-id stream1] [--host 127.0.0.1] [--port 1935] "
           "[--app live] [--input /path/a.mp4] [--fps 30] [--width 1280] [--height 720] [--audio-freq 1000]";
}
