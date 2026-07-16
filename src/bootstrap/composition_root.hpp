#pragma once
#include "vynexos/core/logger.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/config_manager.hpp"
#include "vynexos/core/service_manager.hpp"
#include "vynexos/core/ipc_framework.hpp"
#include "vynexos/desktop/display_abstraction.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include "vynexos/hal/input_driver.hpp"
#include "../apps/demo_app.hpp"
#include "../apps/desktop_shell.hpp"
#include "../apps/basic_terminal.hpp"
#include "../apps/basic_file_explorer.hpp"
#include "../core/basic_plugin_manager.hpp"
#include "../hal/mock_audio_driver.hpp"
#include "../hal/mock_compute_driver.hpp"
#include "../desktop/basic_notification_service.hpp"
#include <memory>
#include <atomic>

namespace vynexos::bootstrap {

// The Composition Root wires all abstract dependencies to their concrete implementations.
class CompositionRoot {
public:
    CompositionRoot();
    ~CompositionRoot();

    void initialize();
    void run();
    void shutdown();
    
    void request_stop();

private:
    std::shared_ptr<core::ILogger> m_logger;
    std::shared_ptr<core::IEventBus> m_event_bus;
    std::shared_ptr<core::IConfigManager> m_config_manager;
    std::shared_ptr<core::IServiceManager> m_service_manager;
    std::shared_ptr<core::IIpcFramework> m_ipc_framework;
    std::shared_ptr<desktop::IDisplayAbstraction> m_display_backend;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<desktop::IWindowManager> m_window_manager;
    std::shared_ptr<desktop::IWidgetToolkit> m_widget_toolkit;
    std::shared_ptr<hal::IInputDriver> m_input_driver;
    std::shared_ptr<apps::DemoApp> m_demo_app;
    std::shared_ptr<apps::IDesktopShell> m_desktop_shell;
    std::shared_ptr<apps::BasicTerminal> m_terminal;
    std::shared_ptr<apps::BasicFileExplorer> m_file_explorer;
    
    std::shared_ptr<core::IPluginManager> m_plugin_manager;
    std::shared_ptr<hal::IAudioDevice> m_audio_driver;
    std::shared_ptr<hal::IComputeDevice> m_compute_driver;
    std::shared_ptr<desktop::INotificationService> m_notification_service;
    
    std::atomic<bool> m_is_running{false};
};

} // namespace vynexos::bootstrap
