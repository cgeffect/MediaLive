#pragma once

#include <string>

struct TaskConfig {
    std::string stream_id = "stream1";
    std::string host = "127.0.0.1";
    int port = 1935;
    std::string app = "live";
    std::string input_file;
    int width = 1280;
    int height = 720;
    int fps = 30;
    int audio_freq = 1000;
};

std::string BuildTargetUrl(const TaskConfig &cfg);
TaskConfig ParseArgs(int argc, char **argv);
std::string UsageText();
