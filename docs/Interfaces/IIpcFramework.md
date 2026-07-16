<!-- AUTO-GENERATED START -->
# [[IIpcFramework]]

## Purpose
Interface for [[IIpcFramework]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/ipc_framework.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IIpcFramework]]() = default`
- `virtual std::expected<std::shared_ptr<[[IIpcEndpoint]]>, [[IpcError]]> create_server(std::string_view name) = 0`
- `virtual std::expected<std::shared_ptr<[[IIpcEndpoint]]>, [[IpcError]]> connect_client(std::string_view name) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
