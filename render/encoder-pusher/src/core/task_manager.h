#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "task_config.h"

class TaskManager {
public:
    TaskManager() = default;
    ~TaskManager();

    bool StartTask(const TaskConfig &cfg);
    void StopAll();
    void ListTasks() const;

private:
    struct TaskState {
        TaskConfig config;
        std::atomic<bool> running{false};
        std::thread worker;
    };

    static void RunTask(TaskState &state);

    mutable std::mutex mu_;
    std::map<std::string, std::unique_ptr<TaskState>> tasks_;
};
