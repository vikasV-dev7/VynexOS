<!-- AUTO-GENERATED START -->
# [[ILogger]]

## Why the interface exists & Architectural role
Defines the standard logging interface for the VynexOS ecosystem.

## Lifetime & Ownership expectations
**Lifetime**: _Unspecified_
**Ownership**: _Unspecified_

## Threading guarantees & Failure behaviour
**Thread Safety**: Implementations MUST be thread-safe for concurrent cross-thread logging.
**Failure Handling**: _Unspecified_

## Extension guidance
_No explicit extension points defined._

## Public API
- `virtual ~[[ILogger]]() = default;`
- `virtual void set_log_level([[LogLevel]] level) = 0;`
- `[[nodiscard]] virtual [[LogLevel]] get_log_level() const = 0;`
- `virtual void log_raw([[LogLevel]] level, std::string_view message) = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.196144+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/logger.hpp`

<!-- AUTO-GENERATED END -->










