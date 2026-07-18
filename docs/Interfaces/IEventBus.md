<!-- AUTO-GENERATED START -->
# [[IEventBus]]

## Why the interface exists & Architectural role
The central nervous system for decoupled inter-module communication.

## Lifetime & Ownership expectations
**Lifetime**: _Unspecified_
**Ownership**: _Unspecified_

## Threading guarantees & Failure behaviour
**Thread Safety**: Subscribers must be added strictly on the main thread. Publish is thread-safe.
**Failure Handling**: _Unspecified_

## Extension guidance
_No explicit extension points defined._

## Public API
- `virtual ~[[IEventBus]]() = default;`
- `virtual void publish(std::shared_ptr<const Event> event) = 0;`
- `virtual void subscribe(const std::string& topic, EventHandler handler) = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:39.585438+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/event_bus.hpp`

<!-- AUTO-GENERATED END -->


