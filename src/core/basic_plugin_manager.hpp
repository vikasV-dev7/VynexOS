#pragma once
#include "vynexos/core/plugin_manager.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/hal/library_loader.hpp"
#include "vynexos/core/plugin_context_factory.hpp"
#include "vynexos/core/loaded_plugin.hpp"
#include <mutex>
#include <unordered_map>
#include <memory>
#include <string>

namespace vynexos::core {

class BasicPluginManager final : public IPluginManager {
public:
    BasicPluginManager(std::shared_ptr<ILogger> logger,
                       std::unique_ptr<hal::ILibraryLoader> loader,
                       std::unique_ptr<PluginContextFactory> context_factory);
                       
    ~BasicPluginManager() override;

    std::expected<std::shared_ptr<IPlugin>, PluginError> load_plugin(std::string_view path) override;
    void unload_plugin(std::string_view name) override;
    std::vector<std::shared_ptr<IPlugin>> get_active_plugins() const override;

private:
    std::shared_ptr<ILogger> m_logger;
    std::unique_ptr<hal::ILibraryLoader> m_loader;
    std::unique_ptr<PluginContextFactory> m_context_factory;
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<LoadedPlugin>> m_plugins;
};

} // namespace vynexos::core
