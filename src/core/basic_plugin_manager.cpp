#include "basic_plugin_manager.hpp"

namespace vynexos::core {

BasicPluginManager::BasicPluginManager(std::shared_ptr<ILogger> logger)
    : m_logger(std::move(logger)) {}

std::expected<std::shared_ptr<IPlugin>, PluginError> BasicPluginManager::load_plugin(std::string_view path) {
    std::lock_guard lock(m_mutex);
    m_logger->info("BasicPluginManager: Mock loading plugin from '{}'", path);
    
    // For V1.0 Architecture, we return a mock failure or success depending on path
    // Since dynamic library loading is platform specific, this is mocked out.
    m_logger->warning("BasicPluginManager: Dynamic loading is mocked. Returning SandboxViolation.");
    return std::unexpected(PluginError::SandboxViolation);
}

void BasicPluginManager::unload_plugin(std::string_view name) {
    std::lock_guard lock(m_mutex);
    auto it = m_plugins.find(std::string(name));
    if (it != m_plugins.end()) {
        m_logger->info("BasicPluginManager: Unloaded plugin '{}'", name);
        m_plugins.erase(it);
    } else {
        m_logger->warning("BasicPluginManager: Could not find plugin '{}' to unload", name);
    }
}

std::vector<std::shared_ptr<IPlugin>> BasicPluginManager::get_active_plugins() const {
    std::lock_guard lock(m_mutex);
    std::vector<std::shared_ptr<IPlugin>> active;
    active.reserve(m_plugins.size());
    for (const auto& [name, plugin] : m_plugins) {
        active.push_back(plugin);
    }
    return active;
}

} // namespace vynexos::core
