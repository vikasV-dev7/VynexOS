<!-- AUTO-GENERATED START -->
# [[IService]]

## Purpose
Interface for [[IService]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/service_manager.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IService]]() = default`
- `virtual std::string_view get_name() const = 0`
- `virtual std::expected<void, std::string> start() = 0`
- `virtual void stop() = 0`
- `virtual [[ServiceState]] get_state() const = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
