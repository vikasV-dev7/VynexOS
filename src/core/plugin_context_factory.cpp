#include "vynexos/core/plugin_context_factory.hpp"

namespace vynexos::core {

static void c_log_wrapper(int level, const char* message, void* user_data) {
    if (user_data) {
        static_cast<PluginContext*>(user_data)->log(level, message);
    }
}

static bool c_publish_event_wrapper(const char* topic, const uint8_t* payload, size_t size, void* user_data) {
    if (user_data) {
        return static_cast<PluginContext*>(user_data)->publish_event(topic, payload, size);
    }
    return false;
}

static bool c_schedule_task_wrapper(VynexTaskCallback task_func, void* task_data, void* user_data) {
    if (user_data) {
        return static_cast<PluginContext*>(user_data)->schedule_task(task_func, task_data);
    }
    return false;
}

static const char* c_get_config_wrapper(const char* key, void* user_data) {
    if (user_data) {
        return static_cast<PluginContext*>(user_data)->get_configuration(key);
    }
    return nullptr;
}

PluginContextFactory::PluginContextFactory(std::shared_ptr<ILogger> logger,
                                           std::shared_ptr<IEventBus> event_bus,
                                           ITaskScheduler* task_scheduler,
                                           std::shared_ptr<IConfigManager> config_manager)
    : m_logger(std::move(logger)),
      m_event_bus(std::move(event_bus)),
      m_task_scheduler(task_scheduler),
      m_config_manager(std::move(config_manager)) {}

std::unique_ptr<PluginContext> PluginContextFactory::create_cxx_context() {
    return std::make_unique<PluginContext>(m_logger, m_event_bus, m_task_scheduler, m_config_manager);
}

VynexPluginContext PluginContextFactory::create_c_context(PluginContext* cxx_context) {
    VynexPluginContext ctx;
    ctx.internal_context = cxx_context;
    ctx.log = c_log_wrapper;
    ctx.publish_event = c_publish_event_wrapper;
    ctx.schedule_task = c_schedule_task_wrapper;
    ctx.get_configuration = c_get_config_wrapper;
    return ctx;
}

} // namespace vynexos::core
