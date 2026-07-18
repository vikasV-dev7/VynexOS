<!-- AUTO-GENERATED START -->
# [[IIpcEndpoint]]

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
- `virtual ~[[IIpcEndpoint]]() = default;`
- `virtual std::expected<void, [[IpcError]]> send(std::span<const uint8_t> data) = 0;`
- `virtual void register_handler(MessageHandler handler) = 0;`
- `virtual std::string_view get_endpoint_name() const = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.169956+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/ipc_framework.hpp`

<!-- AUTO-GENERATED END -->










