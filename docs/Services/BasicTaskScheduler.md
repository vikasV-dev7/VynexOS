<!-- AUTO-GENERATED START -->
# [[BasicTaskScheduler]]

## Purpose & Responsibilities
_Not explicitly documented._

## Dependencies & Interaction with CompositionRoot
_Constructor injected dependencies (Refer to Source Code)._

## Failure recovery & Performance considerations
**Failure Recovery**: _Unspecified_
**Performance**: _Unspecified_

## Extension points
_No explicit extension points defined._

## Public API
- `~[[BasicTaskScheduler]]() override;`
- `[[nodiscard]] bool enqueue(std::move_only_function<void(const ExecutionContext&)> task) override;`
- `void shutdown() override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:40.023938+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/core/basic_task_scheduler.hpp`

<!-- AUTO-GENERATED END -->



## Purpose & Design Goals
The `[[BasicTaskScheduler]]` provides the core thread-pool and asynchronous task execution engine for VynexOS. Its primary goal is to offload blocking or CPU-intensive work from the main event loop, ensuring the system remains responsive.

## Internal Implementation Overview
The scheduler manages a pool of worker threads (`std::vector<std::jthread>`) sized to match the hardware's concurrency capabilities (or a fallback of 1). Tasks are encapsulated as `std::move_only_function<void(const ExecutionContext&)>` and pushed into a `std::queue`. 
Workers sleep on a `std::condition_variable_any` until a task is enqueued or a shutdown token is signaled. The use of `std::jthread` and `std::stop_token` provides modern C++20 cooperative cancellation semantics.

## Ownership & Lifetime Management
Tasks transfer ownership of their closures (and any captured variables) to the scheduler's queue via `std::move_only_function`. Once a task is popped, the worker thread temporarily owns the closure until execution completes, after which it is destroyed.
The `[[BasicTaskScheduler]]` owns the worker threads. During destruction or an explicit `shutdown()` call, the threads are sent stop requests and joined automatically via `std::jthread` destructors when the `workers_` vector is cleared.

## Thread Safety & Execution Context
The scheduler is the backbone of concurrent execution. The task queue and running state are protected by a `std::mutex`. `enqueue()` can be called safely from any thread, including worker threads themselves (allowing tasks to chain other tasks).
Worker threads execute in an isolated `ExecutionContext`. They run concurrently without locks across the actual task invocations.

## Failure Handling & Error Recovery
Worker threads catch all standard exceptions (`std::exception`) as well as unknown exceptions (`...`) thrown by user tasks. Instead of crashing the entire operating system, these exceptions are intercepted, logged as errors, and the worker thread recovers to process the next task.
If `enqueue()` is called after `shutdown()` has been initiated, the scheduler safely rejects the task by discarding it and returning `false`.

## Testing Strategy
The `[[BasicTaskScheduler]]` can be tested by enqueuing a series of deterministic operations (e.g., atomic counter increments) and waiting for them to complete. Mock tasks are used to verify that the scheduler properly drains the queue on shutdown and correctly handles throwing tasks without terminating the worker threads.

## Extension Points & Future Roadmap
- **Task Prioritization:** Currently, tasks are processed in a pure FIFO manner. Future improvements will introduce priority queues (e.g., High, Normal, Background) to ensure critical system events are processed ahead of long-running compute tasks.
- **Task Continuation:** Adding support for `std::future` or chained callbacks to allow subsystems to cleanly wait on asynchronous task completion.
