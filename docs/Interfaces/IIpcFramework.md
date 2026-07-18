<!-- AUTO-GENERATED START -->
# [[IIpcFramework]]

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
- `virtual ~[[IIpcFramework]]() = default;`
- `virtual std::expected<std::shared_ptr<[[IIpcEndpoint]]>, [[IpcError]]> create_server(std::string_view name) = 0;`
- `virtual std::expected<std::shared_ptr<[[IIpcEndpoint]]>, [[IpcError]]> connect_client(std::string_view name) = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.183510+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/ipc_framework.hpp`

<!-- AUTO-GENERATED END -->










