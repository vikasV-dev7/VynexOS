#pragma once
#include <string_view>
#include <string>
#include <cstdint>

namespace vynexos::desktop {

enum class NotificationPriority {
    Low,
    Normal,
    High,
    Critical
};

struct Notification {
    uint32_t id;
    std::string title;
    std::string body;
    NotificationPriority priority;
};

class INotificationService {
public:
    virtual ~INotificationService() = default;
    
    // Dispatches a notification to the desktop shell
    virtual uint32_t notify(std::string_view title, std::string_view body, NotificationPriority priority = NotificationPriority::Normal) = 0;
    virtual void dismiss(uint32_t id) = 0;
};

} // namespace vynexos::desktop
