#include "vynexos/core/plugin_context.hpp"

namespace vynexos::core {

PluginContext::PluginContext(std::shared_ptr<ILogger> logger,
                             std::shared_ptr<IEventBus> event_bus,
                             ITaskScheduler* task_scheduler,
                             std::shared_ptr<IConfigManager> config_manager)
    : m_logger(std::move(logger)),
      m_event_bus(std::move(event_bus)),
      m_task_scheduler(task_scheduler),
      m_config_manager(std::move(config_manager)) {}

void PluginContext::log(int level, const char* message) {
    if (!m_logger) return;
    
    // Simplistic mapping for demonstration
    if (level == 0) m_logger->debug("{}", message);
    else if (level == 1) m_logger->info("{}", message);
    else if (level == 2) m_logger->warning("{}", message);
    else if (level == 3) m_logger->error("{}", message);
    else m_logger->fatal("{}", message);
}

bool PluginContext::publish_event(const char* topic, const uint8_t* payload, size_t size) {
    if (!m_event_bus) return false;
    
    auto event = std::make_shared<Event>();
    event->topic = topic;
    event->payload = std::vector<uint8_t>(payload, payload + size);
    
    m_event_bus->publish(event);
    return true;
}

bool PluginContext::schedule_task(void (*task_func)(void*), void* task_data) {
    if (!m_task_scheduler) return false;
    
    return m_task_scheduler->enqueue([task_func, task_data](const ExecutionContext&) {
        task_func(task_data);
    });
}

const char* PluginContext::get_configuration(const char* key) {
    if (!m_config_manager) return nullptr;
    
    std::string k(key);
    auto val = m_config_manager->get_value(k);
    if (!val) return nullptr;
    
    if (val->type() == typeid(std::string)) {
        m_config_cache[k] = std::any_cast<std::string>(*val);
        return m_config_cache[k].c_str();
    }
    return nullptr;
}

} // namespace vynexos::core
