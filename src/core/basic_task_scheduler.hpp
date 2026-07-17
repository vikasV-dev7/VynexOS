#pragma once

#include "vynexos/core/task_scheduler.hpp"
#include "vynexos/core/logger.hpp"

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace vynexos::core {

class BasicTaskScheduler final : public ITaskScheduler {
public:
    explicit BasicTaskScheduler(ILogger& logger, size_t thread_count = std::thread::hardware_concurrency());
    ~BasicTaskScheduler() override;

    // Delete copy/move
    BasicTaskScheduler(const BasicTaskScheduler&) = delete;
    BasicTaskScheduler& operator=(const BasicTaskScheduler&) = delete;
    BasicTaskScheduler(BasicTaskScheduler&&) = delete;
    BasicTaskScheduler& operator=(BasicTaskScheduler&&) = delete;

    [[nodiscard]] bool enqueue(std::move_only_function<void(const ExecutionContext&)> task) override;
    void shutdown() override;

private:
    void worker_loop(std::stop_token stoken, size_t thread_id);

    ILogger& logger_;
    std::vector<std::jthread> workers_;
    std::queue<std::move_only_function<void(const ExecutionContext&)>> tasks_;
    
    std::mutex mutex_;
    std::condition_variable_any cv_; // Used with jthread's stop_token
    std::atomic<bool> is_running_{true};
};

} // namespace vynexos::core
