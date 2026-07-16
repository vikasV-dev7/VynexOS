#include "basic_notification_service.hpp"
#include <any>

namespace vynexos::desktop {

BasicNotificationService::BasicNotificationService(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger)
    : m_event_bus(std::move(event_bus)), m_logger(std::move(logger)) {}

uint32_t BasicNotificationService::notify(std::string_view title, std::string_view body, NotificationPriority priority) {
    std::lock_guard lock(m_mutex);
    uint32_t id = ++m_next_id;
    Notification n{id, std::string(title), std::string(body), priority};
    m_active_notifications[id] = n;
    
    m_event_bus->publish(std::make_shared<core::Event>(core::Event{"notification_show", std::make_any<Notification>(n)}));
    m_logger->info("BasicNotificationService: Notification #{} '{}' published", id, title);
    
    return id;
}

void BasicNotificationService::dismiss(uint32_t id) {
    std::lock_guard lock(m_mutex);
    if (m_active_notifications.erase(id)) {
        m_event_bus->publish(std::make_shared<core::Event>(core::Event{"notification_dismiss", std::make_any<uint32_t>(id)}));
        m_logger->info("BasicNotificationService: Notification #{} dismissed", id);
    }
}

} // namespace vynexos::desktop
