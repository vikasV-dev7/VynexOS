<!-- AUTO-GENERATED START -->
# [[IIpcEndpoint]]

## Purpose
Interface for [[IIpcEndpoint]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/ipc_framework.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IIpcEndpoint]]() = default`
- `virtual std::expected<void, [[IpcError]]> send(std::span<const uint8_t> data) = 0`
- `virtual void register_handler(MessageHandler handler) = 0`
- `virtual std::string_view get_endpoint_name() const = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
