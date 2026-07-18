<!-- AUTO-GENERATED START -->
# [[InMemoryEventBus]]

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
- `~[[InMemoryEventBus]]() override = default;`
- `void subscribe(const std::string& topic, EventHandler handler) override {`
- `void publish(std::shared_ptr<const Event> event) override {`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:40.047318+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/core/in_memory_event_bus.hpp`

<!-- AUTO-GENERATED END -->


