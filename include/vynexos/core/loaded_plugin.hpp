#pragma once
#include "vynexos/core/plugin_manager.hpp"
#include "vynexos/core/plugin_context.hpp"
#include "vynexos/hal/library_loader.hpp"
#include "vynexos_abi/plugin.h"
#include <memory>
#include <string>

namespace vynexos::core {

enum class PluginState {
    Loaded,
    Created,
    Initialized,
    ServicesRegistered,
    Started,
    Running,
    Stopped,
    Shutdown,
    Destroyed
};

class LoadedPlugin final : public IPlugin {
public:
    LoadedPlugin(std::shared_ptr<hal::ISharedLibrary> library,
                 VynexPluginManifest manifest,
                 std::unique_ptr<PluginContext> context,
                 VynexPluginContext c_context);
                 
    ~LoadedPlugin() override;

    // IPlugin Implementation
    std::string_view get_name() const override;
    std::string_view get_description() const override;

    // Check if ABI symbols are fully resolved
    bool has_valid_abi() const;

    // Lifecycle transitions controlled by PluginManager
    bool transition_create();
    bool transition_initialize();
    bool transition_register_services();
    bool transition_start();
    void transition_to_running();

    void transition_stop();
    void transition_shutdown();
    void transition_destroy();

    PluginState get_state() const { return m_state; }

private:
    std::shared_ptr<hal::ISharedLibrary> m_library;
    VynexPluginManifest m_manifest;
    std::unique_ptr<PluginContext> m_cxx_context;
    VynexPluginContext m_c_context;
    PluginState m_state{PluginState::Loaded};

    typedef bool (*CreateFunc)(void);
    typedef bool (*InitFunc)(const VynexPluginContext*);
    typedef bool (*RegServicesFunc)(void);
    typedef bool (*StartFunc)(void);
    typedef void (*StopFunc)(void);
    typedef void (*ShutdownFunc)(void);
    typedef void (*DestroyFunc)(void);

    CreateFunc fn_create{nullptr};
    InitFunc fn_init{nullptr};
    RegServicesFunc fn_reg{nullptr};
    StartFunc fn_start{nullptr};
    StopFunc fn_stop{nullptr};
    ShutdownFunc fn_shutdown{nullptr};
    DestroyFunc fn_destroy{nullptr};

    void load_symbols();
};

} // namespace vynexos::core
