<!-- AUTO-GENERATED START -->
# [[LocalIpcFramework]]

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
- `std::expected<std::shared_ptr<[[IIpcEndpoint]]>, [[IpcError]]> create_server(std::string_view name) override;`
- `std::expected<std::shared_ptr<[[IIpcEndpoint]]>, [[IpcError]]> connect_client(std::string_view name) override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.574081+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/core/local_ipc_framework.hpp`

<!-- AUTO-GENERATED END -->










