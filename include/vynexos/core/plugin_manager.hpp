#pragma once
#include <string_view>
#include <expected>
#include <vector>
#include <memory>

namespace vynexos::core {

enum class PluginError {
    FileNotFound,
    InvalidABI,
    VersionMismatch,
    SandboxViolation,
    InitFailed,
    DuplicatePlugin
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    virtual std::string_view get_name() const = 0;
    virtual std::string_view get_description() const = 0;
};

class IPluginManager {
public:
    virtual ~IPluginManager() = default;
    
    // Loads a plugin library (.so/.dll) and enforces ABI and sandboxing
    virtual std::expected<std::shared_ptr<IPlugin>, PluginError> load_plugin(std::string_view path) = 0;
    
    // Unloads a loaded plugin
    virtual void unload_plugin(std::string_view name) = 0;
    
    // Retrieve all active plugins
    virtual std::vector<std::shared_ptr<IPlugin>> get_active_plugins() const = 0;
};

} // namespace vynexos::core
