#include "basic_task_scheduler.hpp"
#include <exception>

namespace vynexos::core {

BasicTaskScheduler::BasicTaskScheduler(ILogger& logger, size_t thread_count)
    : logger_(logger) {
    
    if (thread_count == 0) {
        thread_count = 1; // Fallback if hardware_concurrency returns 0
    }

    logger_.info("BasicTaskScheduler: Initializing {} worker threads.", thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this, i](std::stop_token stoken) {
            this->worker_loop(std::move(stoken), i);
        });
    }
}

BasicTaskScheduler::~BasicTaskScheduler() {
    shutdown();
}

bool BasicTaskScheduler::enqueue(std::move_only_function<void(const ExecutionContext&)> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_running_) {
            logger_.warning("BasicTaskScheduler: Enqueue called while shutting down. Task discarded.");
            return false;
        }
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
    return true;
}

void BasicTaskScheduler::shutdown() {
    bool expected = true;
    if (is_running_.compare_exchange_strong(expected, false)) {
        logger_.info("BasicTaskScheduler: Initiating shutdown. Draining queue...");
        
        // Notify all threads to wake up and process remaining tasks or exit
        for (auto& worker : workers_) {
            worker.request_stop();
        }
        cv_.notify_all();

        // std::jthread destructors in workers_.clear() will automatically join()
        workers_.clear(); 
        
        logger_.info("BasicTaskScheduler: Shutdown complete.");
    }
}

void BasicTaskScheduler::worker_loop(std::stop_token stoken, size_t thread_id) {
    logger_.debug("BasicTaskScheduler: Worker thread {} started.", thread_id);
    
    ExecutionContext context{};

    while (true) {
        std::move_only_function<void(const ExecutionContext&)> task;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // Wait until a task is available OR a stop is requested (which sets stop_token)
            cv_.wait(lock, stoken, [this]() {
                return !tasks_.empty();
            });

            // If stop requested and queue is empty, exit loop.
            // If stop requested but queue has tasks, we drain them first.
            if (stoken.stop_requested() && tasks_.empty()) {
                break;
            }

            // This can happen if notified but queue is empty (spurious wakeup without stop)
            if (tasks_.empty()) {
                continue;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        // Execute task without holding the lock
        try {
            if (task) {
                task(context);
            }
        } catch (const std::exception& e) {
            logger_.error("BasicTaskScheduler: Worker thread caught exception: {}", e.what());
        } catch (...) {
            logger_.error("BasicTaskScheduler: Worker thread caught unknown exception.");
        }
    }

    logger_.debug("BasicTaskScheduler: Worker thread {} exiting.", thread_id);
}

} // namespace vynexos::core
