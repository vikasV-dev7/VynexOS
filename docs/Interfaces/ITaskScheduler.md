<!-- AUTO-GENERATED START -->
# [[ITaskScheduler]]

## Purpose
Interface for [[ITaskScheduler]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/task_scheduler.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[ITaskScheduler]]() = default`
- `[[nodiscard]] virtual bool enqueue(std::move_only_function<void(const ExecutionContext&)> task) = 0`
- `virtual void shutdown() = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
