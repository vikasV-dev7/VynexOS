#include "composition_root.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../core/in_memory_config.hpp"
#include "../core/basic_service_manager.hpp"
#include "../core/file_logger.hpp"
#include "../core/local_ipc_framework.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../desktop/mock_display_backend.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../hal/mock_input_driver.hpp"
#ifdef VYNEX_ENABLE_SDL2
#include "../desktop/sdl2_display_backend.hpp"
#include "../hal/sdl2_input_driver.hpp"
#endif
#include "vynexos/core/frame_clock.hpp"
#include <thread>
#include <vynexos/hal/library_loader.hpp>
#include <vynexos/core/plugin_context_factory.hpp>
#include <vynexos/core/memory_file_system.hpp>
#include <vynexos/apps/application_registry.hpp>
#include "../desktop/basic_taskbar.hpp"
#include "../apps/basic_file_explorer.hpp"
#include "../apps/basic_terminal.hpp"
#include "../apps/demo_app.hpp"

namespace vynexos::bootstrap {

CompositionRoot::CompositionRoot() {
    // 1. Instantiate concrete implementations
    m_logger = std::make_shared<core::FileLogger>("vynexos_runtime.log");
    m_task_scheduler = std::make_unique<core::BasicTaskScheduler>(*m_logger);
    m_event_bus = std::make_shared<core::InMemoryEventBus>(*m_task_scheduler);
    m_config_manager = std::make_shared<core::InMemoryConfigManager>();
    m_ipc_framework = std::make_shared<core::LocalIpcFramework>();
    
#ifdef VYNEX_ENABLE_SDL2
    // The display backend owns the window geometry, so it also maps pointer coordinates
    // from the window into the logical desktop space.
    auto sdl_display = std::make_shared<desktop::SDL2DisplayBackend>(m_logger);
    m_display_backend = sdl_display;
    m_compositor = std::make_shared<desktop::BasicCompositor>(m_display_backend, m_logger);
    m_input_driver = std::make_shared<hal::SDL2InputDriver>(m_event_bus, m_logger, sdl_display);
#else
    m_display_backend = std::make_shared<desktop::MockDisplayBackend>(m_logger);
    m_compositor = std::make_shared<desktop::BasicCompositor>(m_display_backend, m_logger);
    m_input_driver = std::make_shared<hal::MockInputDriver>(m_event_bus, m_logger);
#endif
    
    m_window_manager = std::make_shared<desktop::BasicWindowManager>(m_event_bus, m_logger);
    m_widget_toolkit = std::make_shared<desktop::BasicWidgetToolkit>();
    auto decorator = std::make_shared<desktop::BasicWindowDecorator>(m_widget_toolkit);
    auto taskbar = std::make_shared<desktop::BasicTaskbar>(m_widget_toolkit);
    m_desktop_shell = std::make_shared<apps::DesktopShell>(m_window_manager, m_compositor, m_widget_toolkit, decorator, taskbar, m_event_bus);

    // New V1.0 Architecture Components
    auto lib_loader = hal::create_library_loader();
    auto ctx_factory = std::make_unique<core::PluginContextFactory>(m_logger, m_event_bus, m_task_scheduler.get(), m_config_manager);
    m_plugin_manager = std::make_shared<core::BasicPluginManager>(m_logger, std::move(lib_loader), std::move(ctx_factory));
    
    m_audio_driver = std::make_shared<hal::MockAudioDriver>(m_logger);
    m_compute_driver = std::make_shared<hal::MockComputeDriver>(m_logger);
    m_network_adapter = std::make_shared<hal::MockNetworkAdapter>(m_logger);
    m_block_device = std::make_shared<hal::MockBlockDevice>(m_logger, 1024 * 1024); // 512MB
    m_hardware_clock = std::make_shared<hal::MockHardwareClock>(m_logger);
    
    m_notification_service = std::make_shared<desktop::BasicNotificationService>(m_event_bus, m_logger);
    
    // Inject logger into Service Manager
    m_service_manager = std::make_shared<core::BasicServiceManager>(m_logger);
    
    // Phase 3 components
    m_vfs = std::make_shared<core::MemoryFileSystem>();
    m_app_registry = std::make_shared<apps::ApplicationRegistry>();
    m_app_host = std::make_shared<apps::ApplicationHost>(m_app_registry, m_window_manager, m_compositor, m_desktop_shell);
    m_desktop_shell->set_application_host(m_app_host.get());

    // The applications the desktop offers (launcher order).
    m_app_host->register_application({"Terminal", "Terminal", {100, 100, 600, 400}},
                                     [toolkit = m_widget_toolkit]() { return std::make_unique<apps::BasicTerminal>(toolkit); });
    m_app_host->register_application({"BasicFileExplorer", "File Explorer", {200, 200, 700, 500}},
                                     [vfs = m_vfs, toolkit = m_widget_toolkit]() { return std::make_unique<apps::BasicFileExplorer>(vfs, toolkit); });
    m_app_host->register_application({"DemoApp", "Demo App", {400, 300, 300, 200}},
                                     [toolkit = m_widget_toolkit]() { return std::make_unique<apps::DemoApp>(toolkit); });
}

CompositionRoot::~CompositionRoot() = default;

void CompositionRoot::initialize() {
    m_logger->info("VynexOS Bootstrap: Initializing Core Services...");
    if (auto res = m_display_backend->initialize(); !res) {
        m_logger->fatal("Failed to initialize Display Backend!");
    }
    
    if (auto res = m_audio_driver->initialize(); !res) {
        m_logger->error("Failed to initialize Audio Backend!");
    }
    
    if (auto res = m_compute_driver->initialize(); !res) {
        m_logger->error("Failed to initialize Compute Backend!");
    }
    
    if (auto res = m_hardware_clock->initialize(); !res) {
        m_logger->error("Failed to initialize Hardware Clock!");
    }
    
    if (auto res = m_block_device->initialize(); !res) {
        m_logger->error("Failed to initialize Block Device!");
    }
    
    if (auto res = m_network_adapter->initialize(); !res) {
        m_logger->error("Failed to initialize Network Adapter!");
    }
    
    m_logger->info("VynexOS Bootstrap: Launching Desktop Shell...");
    m_desktop_shell->launch();
    
    m_logger->info("VynexOS Bootstrap: Launching System Apps...");
    for (const char* app_id : {"Terminal", "BasicFileExplorer", "DemoApp"}) {
        if (!m_app_host->open(app_id)) m_logger->error("Failed to launch {}", app_id);
    }
}

void CompositionRoot::run() {
    m_logger->info("VynexOS Bootstrap: Starting all services...");
    if (auto res = m_service_manager->start_all(); !res) {
        m_logger->error("Boot failed: {}", res.error());
        return;
    }
    m_logger->info("VynexOS Runtime is now active.");
    
    m_is_running = true;
    
    // Dynamic cast to access internal polling method
    auto local_ipc = std::static_pointer_cast<core::LocalIpcFramework>(m_ipc_framework);
    
    core::FrameClock frame_clock(60);
    
    // Main Event Loop
    while (m_is_running) {
        frame_clock.begin_frame();
        
        m_input_driver->poll();
        if (m_input_driver->is_shutdown_requested()) {
            m_logger->info("CompositionRoot: HAL reports shutdown requested. Breaking run loop.");
            break;
        }
        local_ipc->poll_messages();
        
        // Let apps update their buffers using the toolkit
        m_desktop_shell->update_frame();

        // WindowManager builds the scene graph
        auto scene = m_window_manager->build_scene();
        
        auto start_render = std::chrono::steady_clock::now();
        // Compositor handles rendering the scene graph to the display backend
        if (auto res = m_compositor->render_frame(scene); !res) {
            m_logger->error("Compositor dropped frame due to display error.");
        }
        auto end_render = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end_render - start_render).count();
        
        static uint64_t frame_count = 0;
        frame_count++;
        if (frame_count % 60 == 0) { 
            m_logger->info("Frame Loop: Frame={}, SceneSize={}, RenderDur={}us", frame_count, scene.size(), dur);
        }
        
        frame_clock.end_frame_and_wait();
    }
}

void CompositionRoot::request_stop() {
    m_logger->info("VynexOS Bootstrap: Stop requested.");
    m_is_running = false;
}

void CompositionRoot::shutdown() {
    m_logger->info("VynexOS Bootstrap: Shutting down...");
    
    // =========================================================================
    // EXPLICIT LOGICAL SHUTDOWN SEQUENCE
    // =========================================================================
    // This phase gracefully halts active operations in strict dependency order.
    // Actual memory release is deferred to standard C++ RAII object destruction 
    // after this function completes.
    
    // 1. Stop all high-level services first. They may enqueue final cleanup tasks.
    m_service_manager->stop_all();
    
    // 2. Explicit Reverse-Order Teardown of components
    m_logger->info("VynexOS Bootstrap: Tearing down services in reverse order...");
    m_service_manager.reset();
    m_notification_service.reset();
    
    m_hardware_clock->shutdown();
    m_hardware_clock.reset();
    m_block_device->shutdown();
    m_block_device.reset();
    m_network_adapter->shutdown();
    m_network_adapter.reset();
    m_compute_driver.reset();
    m_audio_driver.reset();
    
    m_plugin_manager.reset();
    
    // Terminate any registered applications before tearing down core components
    m_logger->info("VynexOS Bootstrap: Terminating registered applications...");
    
    // Explicit Desktop Runtime Shutdown Sequence: close every hosted application (window, instance, surface), then
    // detach the host so the shell can no longer reach it.
    if (m_app_host) {
        m_app_host->close_all();
        if (m_desktop_shell) m_desktop_shell->set_application_host(nullptr);
        m_app_host.reset();
    }
    if (m_desktop_shell) {
        m_desktop_shell->clear_running_applications();
    }

    if (m_app_registry) {
        m_app_registry->terminate_all();
        m_app_registry.reset();
    }
    
    // Apps
    m_desktop_shell.reset();

    // VFS can be safely torn down once no apps are holding file handles
    m_vfs.reset();
    
    // Desktop core
    m_widget_toolkit.reset();
    m_window_manager.reset();
    
    // HALs
    m_input_driver.reset();
    m_compositor.reset();
    
    if (m_display_backend) {
        m_display_backend->shutdown();
        m_display_backend.reset();
    }
    
    m_ipc_framework.reset();
    m_config_manager.reset();
    m_event_bus.reset();
    
    // Finally, shut down the Task Scheduler.
    if (m_task_scheduler) {
        m_task_scheduler->shutdown();
        m_task_scheduler.reset();
    }
    
    m_logger->info("Logical shutdown complete. VynexOS Terminated.");
    m_logger.reset();
}

} // namespace vynexos::bootstrap
