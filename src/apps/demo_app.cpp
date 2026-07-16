#include "demo_app.hpp"

namespace vynexos::apps {

DemoApp::DemoApp(std::shared_ptr<desktop::IWindowManager> wm,
                 std::shared_ptr<desktop::ICompositor> compositor,
                 std::shared_ptr<desktop::IWidgetToolkit> toolkit)
    : m_wm(std::move(wm)), m_compositor(std::move(compositor)), m_toolkit(std::move(toolkit)) {}

void DemoApp::launch() {
    // 1. Ask Window Manager to create window logic
    desktop::WindowGeometry geom{100, 100, 640, 480};
    m_window_id = m_wm->create_window("VynexOS Demo App", geom);
    m_wm->focus_window(m_window_id);
    
    // 2. Initialize pixel buffer for Compositor
    m_buffer.id = m_window_id;
    m_buffer.x = geom.x;
    m_buffer.y = geom.y;
    m_buffer.width = geom.width;
    m_buffer.height = geom.height;
    m_buffer.z_index = 1; // In a full OS, WM would sync this to Compositor
    m_buffer.pixels.resize(geom.width * geom.height * 4, static_cast<uint8_t>(255)); // White background
}

void DemoApp::update_frame() {
    if (m_window_id == 0) return;
    
    // Clear buffer to white (255)
    std::fill(m_buffer.pixels.begin(), m_buffer.pixels.end(), static_cast<uint8_t>(255));
    
    // Use toolkit to draw UI
    m_toolkit->draw_panel(m_buffer, 0, 0, m_buffer.width, 30); // Title bar
    m_toolkit->draw_text(m_buffer, 10, 10, "VynexOS Demo App", desktop::Color{255, 255, 255, 255});
    
    m_toolkit->draw_button(m_buffer, 20, 50, 120, 40, "Click Me");
    
    // Submit to Compositor
    m_compositor->submit_buffer(m_buffer);
}

} // namespace vynexos::apps
