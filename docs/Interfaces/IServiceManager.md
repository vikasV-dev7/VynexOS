<!-- AUTO-GENERATED START -->
# [[IServiceManager]]

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
- `virtual ~[[IServiceManager]]() = default;`
- `virtual void register_service(std::shared_ptr<[[IService]]> service) = 0;`
- `virtual std::expected<void, std::string> start_all() = 0;`
- `virtual void stop_all() = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.244277+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/service_manager.hpp`

<!-- AUTO-GENERATED END -->










