#include "basic_plugin_manager.hpp"

namespace vynexos::core {

BasicPluginManager::BasicPluginManager(std::shared_ptr<ILogger> logger,
                                       std::unique_ptr<hal::ILibraryLoader> loader,
                                       std::unique_ptr<PluginContextFactory> context_factory)
    : m_logger(std::move(logger)),
      m_loader(std::move(loader)),
      m_context_factory(std::move(context_factory)) {}

BasicPluginManager::~BasicPluginManager() {
    std::lock_guard lock(m_mutex);
    for (auto& [name, plugin] : m_plugins) {
        m_logger->info("BasicPluginManager: Shutting down plugin '{}'", name);
        plugin->transition_stop();
        plugin->transition_shutdown();
        plugin->transition_destroy();
    }
    m_plugins.clear();
}

std::expected<std::shared_ptr<IPlugin>, PluginError> BasicPluginManager::load_plugin(std::string_view path) {
    std::lock_guard lock(m_mutex);
    m_logger->info("BasicPluginManager: Loading plugin from '{}'", path);
    
    // 1. Load Library
    auto lib_result = m_loader->load_library(path);
    if (!lib_result) {
        m_logger->error("BasicPluginManager: Failed to load library");
        return std::unexpected(PluginError::FileNotFound);
    }
    auto library = lib_result.value();

    // 2. Read Metadata & Validate ABI implicitly via get_manifest
    typedef VynexPluginManifest (*GetManifestFunc)(void);
    auto fn_get_manifest = reinterpret_cast<GetManifestFunc>(library->get_symbol("vynex_plugin_get_manifest"));
    if (!fn_get_manifest) {
        m_logger->error("BasicPluginManager: Missing vynex_plugin_get_manifest");
        return std::unexpected(PluginError::InvalidABI);
    }
    
    VynexPluginManifest manifest = fn_get_manifest();
    
    // Check version (must match 1.x.x for now)
    if (manifest.required_os_version.major != 1) {
        m_logger->error("BasicPluginManager: Version mismatch for plugin '{}'", manifest.plugin_name);
        return std::unexpected(PluginError::VersionMismatch);
    }

    std::string plugin_name(manifest.plugin_name);
    if (m_plugins.find(plugin_name) != m_plugins.end()) {
        m_logger->error("BasicPluginManager: Plugin '{}' is already loaded", plugin_name);
        return std::unexpected(PluginError::DuplicatePlugin);
    }

    // Create contexts
    auto cxx_context = m_context_factory->create_cxx_context();
    VynexPluginContext c_context = m_context_factory->create_c_context(cxx_context.get());

    auto plugin = std::make_shared<LoadedPlugin>(library, manifest, std::move(cxx_context), c_context);

    // 3. Validate ABI (full symbol presence)
    if (!plugin->has_valid_abi()) {
        m_logger->error("BasicPluginManager: Invalid ABI for plugin '{}'", plugin_name);
        return std::unexpected(PluginError::InvalidABI);
    }

    // 4. Resolve Dependencies (Placeholder for future iteration, assume true)
    
    // 5. Create Plugin
    if (!plugin->transition_create()) {
        m_logger->error("BasicPluginManager: Plugin '{}' failed creation", plugin_name);
        return std::unexpected(PluginError::InitFailed);
    }

    // 6. Initialize(Context)
    if (!plugin->transition_initialize()) {
        m_logger->error("BasicPluginManager: Plugin '{}' failed initialization", plugin_name);
        return std::unexpected(PluginError::InitFailed);
    }

    // 7. Register Services
    if (!plugin->transition_register_services()) {
        m_logger->error("BasicPluginManager: Plugin '{}' failed to register services", plugin_name);
        return std::unexpected(PluginError::InitFailed);
    }

    // 8. Start
    if (!plugin->transition_start()) {
        m_logger->error("BasicPluginManager: Plugin '{}' failed to start", plugin_name);
        return std::unexpected(PluginError::InitFailed);
    }

    // 9. Transition to Running
    plugin->transition_to_running();
    m_plugins[plugin_name] = plugin;
    
    m_logger->info("BasicPluginManager: Successfully loaded and started plugin '{}'", plugin_name);
    return plugin;
}

void BasicPluginManager::unload_plugin(std::string_view name) {
    std::lock_guard lock(m_mutex);
    std::string plugin_name(name);
    
    auto it = m_plugins.find(plugin_name);
    if (it != m_plugins.end()) {
        m_logger->info("BasicPluginManager: Unloading plugin '{}'", plugin_name);
        auto plugin = it->second;
        
        // Strict Teardown Lifecycle
        plugin->transition_stop();
        plugin->transition_shutdown();
        plugin->transition_destroy();
        
        m_plugins.erase(it);
    } else {
        m_logger->warning("BasicPluginManager: Could not find plugin '{}' to unload", plugin_name);
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
