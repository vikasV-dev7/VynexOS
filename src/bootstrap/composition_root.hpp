#pragma once
#include "vynexos/core/logger.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/config_manager.hpp"
#include "vynexos/core/task_scheduler.hpp"
#include "vynexos/core/service_manager.hpp"
#include "vynexos/core/ipc_framework.hpp"
#include "vynexos/desktop/display_abstraction.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include "vynexos/hal/input_driver.hpp"
#include "../apps/application_host.hpp"
#include "../apps/desktop_shell.hpp"
#include "startup_options.hpp"
#include "scene_snapshot.hpp"
#include "../core/basic_plugin_manager.hpp"
#include "../hal/mock_audio_driver.hpp"
#include "../hal/mock_compute_driver.hpp"
#include "../hal/mock_network_adapter.hpp"
#include "../hal/mock_block_device.hpp"
#include "../hal/mock_hardware_clock.hpp"
#include "../desktop/basic_notification_service.hpp"
#include "vynexos/core/virtual_file_system.hpp"
#include "vynexos/apps/application_manager.hpp"
#include "vynexos/desktop/running_application.hpp"
#include <memory>
#include <atomic>
#include <vector>

namespace vynexos::bootstrap {

// The Composition Root wires all abstract dependencies to their concrete implementations.
class CompositionRoot {
public:
    /// The default options are those of a plain launch (see StartupOptions).
    explicit CompositionRoot(StartupOptions options = {});
    ~CompositionRoot();

    void initialize();
    void run();
    void shutdown();
    
    void request_stop();

    /// The system's filesystem (owned by the root), for inspection by tests and tools.
    [[nodiscard]] std::shared_ptr<core::IVirtualFileSystem> virtual_file_system() const { return m_vfs; }
    /// The system's task scheduler (owned by the root; null after shutdown), for tests of the shutdown sequence.
    [[nodiscard]] core::ITaskScheduler* task_scheduler() const { return m_task_scheduler.get(); }

private:
    // =========================================================================
    // C++ OBJECT DESTRUCTION ORDER (RAII)
    // =========================================================================
    // The logical operational shutdown sequence is explicitly handled in `shutdown()`.
    // Actual memory release relies on standard C++ RAII (reverse order of declaration).
    // 
    // Foundational Contract for RAII Teardown:
    // 1. Asynchronous work is quiesced first: right after services stop, the Task Scheduler is shut down, which runs
    //    every queued task (event deliveries, plugin tasks) while all the objects they reach are still alive and then
    //    rejects new work. Nothing asynchronous runs during the teardown that follows.
    // 2. Consumers terminate next; subscribers unsubscribe from the EventBus as they are destroyed.
    // 3. EventBus and IPC frameworks terminate before the scheduler object is released.
    // shutdown() runs once: later calls, and the destructor's call, do nothing.
    // =========================================================================

    std::shared_ptr<core::ILogger> m_logger;
    std::unique_ptr<core::ITaskScheduler> m_task_scheduler;
    std::shared_ptr<core::IEventBus> m_event_bus;
    std::shared_ptr<core::IConfigManager> m_config_manager;
    std::shared_ptr<core::IServiceManager> m_service_manager;
    std::shared_ptr<core::IIpcFramework> m_ipc_framework;
    std::shared_ptr<desktop::IDisplayAbstraction> m_display_backend;
    std::shared_ptr<desktop::ICompositor> m_compositor;
    std::shared_ptr<desktop::IWindowManager> m_window_manager;
    std::shared_ptr<desktop::IWidgetToolkit> m_widget_toolkit;
public:
    std::shared_ptr<hal::IInputDriver> m_input_driver;
private:
    std::shared_ptr<apps::IDesktopShell> m_desktop_shell;


    std::shared_ptr<core::IPluginManager> m_plugin_manager;
    std::shared_ptr<hal::IAudioDevice> m_audio_driver;
    std::shared_ptr<hal::IComputeDevice> m_compute_driver;
    std::shared_ptr<hal::INetworkAdapter> m_network_adapter;
    std::shared_ptr<hal::IBlockDevice> m_block_device;
    std::shared_ptr<hal::IHardwareClock> m_hardware_clock;
    std::shared_ptr<desktop::INotificationService> m_notification_service;
    
    std::shared_ptr<core::IVirtualFileSystem> m_vfs;
    std::shared_ptr<apps::IApplicationManager> m_app_registry;
    std::shared_ptr<apps::ApplicationHost> m_app_host; // windows <-> application instances; the shell's launcher and close button use it

    std::unique_ptr<SceneSnapshotWriter> m_scene_snapshot; // only with --scene-snapshot (test/diagnostic interface)

    std::atomic<bool> m_is_running{false};
    bool m_shut_down{false};
};

} // namespace vynexos::bootstrap
