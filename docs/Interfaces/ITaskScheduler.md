<!-- AUTO-GENERATED START -->
# [[ITaskScheduler]]

## Why the interface exists & Architectural role
_Not explicitly documented._

## Lifetime & Ownership expectations
**Lifetime**: _Unspecified_
**Ownership**: _Unspecified_

## Threading guarantees & Failure behaviour
**Thread Safety**: _Unspecified_
**Failure Handling**: _Unspecified_

## Extension guidance
_No explicit extension points defined._

## Public API
- `virtual ~[[ITaskScheduler]]() = default;`
- `[[nodiscard]] virtual bool enqueue(std::move_only_function<void(const ExecutionContext&)> task) = 0;`
- `virtual void shutdown() = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:39.658961+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/task_scheduler.hpp`

<!-- AUTO-GENERATED END -->


