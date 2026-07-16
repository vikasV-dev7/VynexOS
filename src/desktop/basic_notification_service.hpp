#pragma once
#include "vynexos/desktop/notification_service.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace vynexos::desktop {

class BasicNotificationService final : public INotificationService {
public:
    BasicNotificationService(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger);
    ~BasicNotificationService() override = default;

    uint32_t notify(std::string_view title, std::string_view body, NotificationPriority priority = NotificationPriority::Normal) override;
    void dismiss(uint32_t id) override;

private:
    std::shared_ptr<core::IEventBus> m_event_bus;
    std::shared_ptr<core::ILogger> m_logger;
    mutable std::mutex m_mutex;
    
    uint32_t m_next_id{0};
    std::unordered_map<uint32_t, Notification> m_active_notifications;
};

} // namespace vynexos::desktop
