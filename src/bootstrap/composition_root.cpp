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
#include "../hal/mock_input_driver.hpp"
#include <thread>
#include <chrono>
#include <vynexos/hal/library_loader.hpp>
#include <vynexos/core/plugin_context_factory.hpp>

namespace vynexos::bootstrap {

CompositionRoot::CompositionRoot() {
    // 1. Instantiate concrete implementations
    m_logger = std::make_shared<core::FileLogger>("vynexos_runtime.log");
    m_task_scheduler = std::make_unique<core::BasicTaskScheduler>(*m_logger);
    m_event_bus = std::make_shared<core::InMemoryEventBus>(*m_task_scheduler);
    m_config_manager = std::make_shared<core::InMemoryConfigManager>();
    m_ipc_framework = std::make_shared<core::LocalIpcFramework>();
    
    m_display_backend = std::make_shared<desktop::MockDisplayBackend>(m_logger);
    m_compositor = std::make_shared<desktop::BasicCompositor>(m_display_backend, m_logger);
    m_input_driver = std::make_shared<hal::MockInputDriver>(m_event_bus, m_logger);
    
    m_window_manager = std::make_shared<desktop::BasicWindowManager>(m_event_bus, m_logger);
    m_widget_toolkit = std::make_shared<desktop::BasicWidgetToolkit>();
    m_demo_app = std::make_shared<apps::DemoApp>(m_window_manager, m_compositor, m_widget_toolkit);
    m_desktop_shell = std::make_shared<apps::DesktopShell>(m_window_manager, m_compositor, m_widget_toolkit, m_event_bus);
    m_terminal = std::make_shared<apps::BasicTerminal>(m_window_manager, m_compositor, m_widget_toolkit);
    m_file_explorer = std::make_shared<apps::BasicFileExplorer>(m_window_manager, m_compositor, m_widget_toolkit);
    
    // New V1.0 Architecture Components
    auto lib_loader = hal::create_library_loader();
    auto ctx_factory = std::make_unique<core::PluginContextFactory>(m_logger, m_event_bus, m_task_scheduler.get(), m_config_manager);
    m_plugin_manager = std::make_shared<core::BasicPluginManager>(m_logger, std::move(lib_loader), std::move(ctx_factory));
    
    m_audio_driver = std::make_shared<hal::MockAudioDriver>(m_logger);
    m_compute_driver = std::make_shared<hal::MockComputeDriver>(m_logger);
    m_notification_service = std::make_shared<desktop::BasicNotificationService>(m_event_bus, m_logger);
    
    // Inject logger into Service Manager
    m_service_manager = std::make_shared<core::BasicServiceManager>(m_logger);
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
    
    m_logger->info("VynexOS Bootstrap: Launching Desktop Shell...");
    m_desktop_shell->launch();
    
    m_logger->info("VynexOS Bootstrap: Launching System Apps...");
    m_terminal->launch();
    m_file_explorer->launch();
    
    m_logger->info("VynexOS Bootstrap: Launching Demo App...");
    m_demo_app->launch();
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
    
    // Main Event Loop
    int ticks = 0;
    while (m_is_running) {
        m_input_driver->poll();
        local_ipc->poll_messages();
        
        // Simulate a hardware mouse click every 100 ticks to verify focus routing works
        if (++ticks % 100 == 0) {
            auto mock_input = std::static_pointer_cast<hal::MockInputDriver>(m_input_driver);
            mock_input->inject_mouse(150, 150, 1); // Click inside the DemoApp window
            
            // Periodically emit a test notification
            if (ticks % 300 == 0) {
                m_notification_service->notify("System", "Periodic diagnostic event", desktop::NotificationPriority::Normal);
            }
        }
        
        // Let apps update their buffers using the toolkit
        m_desktop_shell->update_frame();
        m_terminal->update_frame();
        m_file_explorer->update_frame();
        m_demo_app->update_frame();
        
        // Compositor handles z-ordering and rendering to the display backend
        if (auto res = m_compositor->render_frame(); !res) {
            m_logger->error("Compositor dropped frame due to display error.");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
    
    // 2. Shut down hardware abstractions. 
    // Cutting off drivers prevents new hardware interrupts from entering the system.
    m_display_backend->shutdown();
    
    // 3. Finally, shut down the Task Scheduler.
    // This blocks and drains the queue, executing any cleanup tasks queued during steps 1 and 2.
    // It must shut down only after all components capable of enqueueing work have stopped.
    if (m_task_scheduler) {
        m_task_scheduler->shutdown();
    }
    
    m_logger->info("Logical shutdown complete. Proceeding to RAII memory release.");
}

} // namespace vynexos::bootstrap
