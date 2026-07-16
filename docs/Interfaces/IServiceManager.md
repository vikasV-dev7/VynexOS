<!-- AUTO-GENERATED START -->
# [[IServiceManager]]

## Purpose
Interface for [[IServiceManager]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/service_manager.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IServiceManager]]() = default`
- `virtual void register_service(std::shared_ptr<[[IService]]> service) = 0`
- `virtual std::expected<void, std::string> start_all() = 0`
- `virtual void stop_all() = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
