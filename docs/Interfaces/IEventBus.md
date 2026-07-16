<!-- AUTO-GENERATED START -->
# [[IEventBus]]

## Purpose
Interface for [[IEventBus]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/event_bus.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IEventBus]]() = default`
- `virtual void publish(std::shared_ptr<const Event> event) = 0`
- `virtual void subscribe(const std::string& topic, EventHandler handler) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
