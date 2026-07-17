#pragma once
#include "vynexos/core/plugin_context.hpp"
#include "vynexos_abi/plugin.h"
#include <memory>

namespace vynexos::core {

class PluginContextFactory {
public:
    PluginContextFactory(std::shared_ptr<ILogger> logger,
                         std::shared_ptr<IEventBus> event_bus,
                         ITaskScheduler* task_scheduler,
                         std::shared_ptr<IConfigManager> config_manager);

    std::unique_ptr<PluginContext> create_cxx_context();
    VynexPluginContext create_c_context(PluginContext* cxx_context);

private:
    std::shared_ptr<ILogger> m_logger;
    std::shared_ptr<IEventBus> m_event_bus;
    ITaskScheduler* m_task_scheduler;
    std::shared_ptr<IConfigManager> m_config_manager;
};

} // namespace vynexos::core
