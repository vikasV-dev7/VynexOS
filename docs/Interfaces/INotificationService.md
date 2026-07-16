<!-- AUTO-GENERATED START -->
# [[INotificationService]]

## Purpose
Interface for [[INotificationService]].

## Namespace
`vynexos::desktop`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/desktop/notification_service.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[INotificationService]]() = default`
- `virtual uint32_t notify(std::string_view title, std::string_view body, [[NotificationPriority]] priority = [[NotificationPriority]]::Normal) = 0`
- `virtual void dismiss(uint32_t id) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->
