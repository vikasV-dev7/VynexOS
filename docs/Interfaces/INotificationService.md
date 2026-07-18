<!-- AUTO-GENERATED START -->
# [[INotificationService]]

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
- `virtual ~[[INotificationService]]() = default;`
- `virtual uint32_t notify(std::string_view title, std::string_view body, [[NotificationPriority]] priority = [[NotificationPriority]]::Normal) = 0;`
- `virtual void dismiss(uint32_t id) = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.271354+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/notification_service.hpp`

<!-- AUTO-GENERATED END -->










