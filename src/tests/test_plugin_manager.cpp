#include "../core/basic_plugin_manager.hpp"
#include "vynexos/core/plugin_context_factory.hpp"
#include "vynexos/hal/library_loader.hpp"
#include "vynexos/core/logger.hpp"
#include "../core/in_memory_config.hpp"
#include <iostream>

using namespace vynexos::core;
using namespace vynexos::hal;

// Dummy classes for dependencies
class DummyLogger : public ILogger {
public:
    void log_raw(LogLevel level, std::string_view message) override {
        (void)level;
        (void)message;
    }
};

int main() {
    auto logger = std::make_shared<DummyLogger>();
    auto config = std::make_shared<InMemoryConfigManager>();
    
    // We can pass nullptr for task scheduler and event bus for these tests since the mock plugins don't use them
    auto factory = std::make_unique<PluginContextFactory>(logger, nullptr, nullptr, config);
    auto loader = create_library_loader();
    
    BasicPluginManager manager(logger, std::move(loader), std::move(factory));

    int failures = 0;
    
#ifdef _WIN32
    std::string ext = ".dll";
#else
    std::string ext = ".so";
#endif

    auto report = [&](const std::string& test_name, bool success) {
        if (success) {
            std::cout << "[PASS] " << test_name << "\n";
        } else {
            std::cout << "[FAIL] " << test_name << "\n";
            failures++;
        }
    };

    // 1. Successful Plugin Loading
    {
        auto res = manager.load_plugin("test_plugin" + ext);
        report("Successful plugin loading", res.has_value());
        
        // Check active plugins
        report("Plugin is active", manager.get_active_plugins().size() == 1);
        
        // Duplicate loading check
        auto res2 = manager.load_plugin("test_plugin" + ext);
        report("Duplicate plugin detection", !res2.has_value() && res2.error() == PluginError::DuplicatePlugin);
        
        // Safe unload
        manager.unload_plugin("ValidTestPlugin");
        report("Safe unload", manager.get_active_plugins().empty());
    }
    
    // 2. Invalid ABI Rejection
    {
        auto res = manager.load_plugin("test_plugin_invalid_abi" + ext);
        report("Invalid ABI rejection", !res.has_value() && res.error() == PluginError::InvalidABI);
    }
    
    // 3. Version Mismatch Rejection
    {
        auto res = manager.load_plugin("test_plugin_v2" + ext);
        report("Version mismatch rejection", !res.has_value() && res.error() == PluginError::VersionMismatch);
    }
    
    // 4. Initialization Failure Cleanup
    {
        auto res = manager.load_plugin("test_plugin_init_fail" + ext);
        report("Initialization failure rejection", !res.has_value() && res.error() == PluginError::InitFailed);
        report("Initialization failure cleanup", manager.get_active_plugins().empty());
    }

    return failures;
}
