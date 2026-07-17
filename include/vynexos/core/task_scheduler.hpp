#pragma once

#include <functional>
#include "vynexos/core/execution_context.hpp"

namespace vynexos::core {

/**
 * @brief Abstract interface for queueing and executing background tasks.
 *
 * Implementations are responsible for managing a worker thread pool.
 */
class ITaskScheduler {
public:
    virtual ~ITaskScheduler() = default;

    /**
     * @brief Enqueues a task for asynchronous execution.
     * @param task The closure to execute. Must not throw exceptions that crash the thread.
     * @return true if successfully queued, false if rejected due to shutdown.
     */
    [[nodiscard]] virtual bool enqueue(std::move_only_function<void(const ExecutionContext&)> task) = 0;

    /**
     * @brief Signals the scheduler to stop accepting new tasks and gracefully shut down.
     */
    virtual void shutdown() = 0;
};

} // namespace vynexos::core
