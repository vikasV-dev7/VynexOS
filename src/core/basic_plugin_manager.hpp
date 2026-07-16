#pragma once
#include "vynexos/core/plugin_manager.hpp"
#include "vynexos/core/logger.hpp"
#include <mutex>
#include <unordered_map>
#include <memory>
#include <string>

namespace vynexos::core {

class BasicPluginManager final : public IPluginManager {
public:
    explicit BasicPluginManager(std::shared_ptr<ILogger> logger);
    ~BasicPluginManager() override = default;

    std::expected<std::shared_ptr<IPlugin>, PluginError> load_plugin(std::string_view path) override;
    void unload_plugin(std::string_view name) override;
    std::vector<std::shared_ptr<IPlugin>> get_active_plugins() const override;

private:
    std::shared_ptr<ILogger> m_logger;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<IPlugin>> m_plugins;
};

} // namespace vynexos::core
