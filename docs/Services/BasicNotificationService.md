<!-- AUTO-GENERATED START -->
# [[BasicNotificationService]]

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
- `~[[BasicNotificationService]]() override = default;`
- `uint32_t notify(std::string_view title, std::string_view body, [[NotificationPriority]] priority = [[NotificationPriority]]::Normal) override;`
- `void dismiss(uint32_t id) override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:40.080150+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/desktop/basic_notification_service.hpp`

<!-- AUTO-GENERATED END -->


