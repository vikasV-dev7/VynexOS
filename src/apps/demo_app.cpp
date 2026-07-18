#include "demo_app.hpp"

namespace vynexos::apps {

DemoApp::DemoApp(std::shared_ptr<desktop::IWindowManager> wm,
                 std::shared_ptr<desktop::ICompositor> compositor,
                 std::shared_ptr<desktop::IWidgetToolkit> toolkit)
    : m_wm(std::move(wm)), m_compositor(std::move(compositor)), m_toolkit(std::move(toolkit)) {}

void DemoApp::launch() {
    desktop::WindowGeometry geom{400, 300, 300, 200};
    m_window_id = m_wm->create_window("Demo App", geom);
    
    m_surface = m_compositor->create_surface(geom.width, geom.height);
    m_wm->set_window_surface(m_window_id, m_surface);
}

void DemoApp::update_frame() {
    if (m_window_id == 0) return;
    
    // Clear surface
    {
        auto map = m_surface->map_pixels();
        std::fill(map.begin(), map.end(), static_cast<uint8_t>(0));
        m_surface->unmap_pixels();
    }
    
    // Draw background
    m_toolkit->draw_panel(m_surface, 0, 0, 300, 200);
    
    // Draw title bar
    m_toolkit->draw_rect(m_surface, 0, 0, 300, 25, desktop::Color{80, 80, 100, 255});
    m_toolkit->draw_text(m_surface, 5, 5, "Demo App", desktop::Color{255, 255, 255, 255});
    
    m_toolkit->draw_button(m_surface, 20, 50, 120, 40, "Click Me");
}

} // namespace vynexos::apps
