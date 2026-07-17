#pragma once
#include "vynexos/core/logger.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/task_scheduler.hpp"
#include "vynexos/core/config_manager.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace vynexos::core {

class PluginContext {
public:
    PluginContext(std::shared_ptr<ILogger> logger,
                  std::shared_ptr<IEventBus> event_bus,
                  ITaskScheduler* task_scheduler,
                  std::shared_ptr<IConfigManager> config_manager);

    void log(int level, const char* message);
    bool publish_event(const char* topic, const uint8_t* payload, size_t size);
    bool schedule_task(void (*task_func)(void*), void* task_data);
    const char* get_configuration(const char* key);

private:
    std::shared_ptr<ILogger> m_logger;
    std::shared_ptr<IEventBus> m_event_bus;
    ITaskScheduler* m_task_scheduler;
    std::shared_ptr<IConfigManager> m_config_manager;
    
    // Cache for returned C-strings
    std::unordered_map<std::string, std::string> m_config_cache;
};

} // namespace vynexos::core
