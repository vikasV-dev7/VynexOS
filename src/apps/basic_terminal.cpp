#include "basic_terminal.hpp"

namespace vynexos::apps {

BasicTerminal::BasicTerminal(std::shared_ptr<desktop::IWindowManager> wm,
                             std::shared_ptr<desktop::ICompositor> compositor,
                             std::shared_ptr<desktop::IWidgetToolkit> toolkit)
    : m_wm(std::move(wm)), m_compositor(std::move(compositor)), m_toolkit(std::move(toolkit)) {
    m_history.push_back("VynexOS Terminal v1.0");
    m_history.push_back("root@vynex:~# _");
}

void BasicTerminal::write_output(std::string_view text) {
    m_history.emplace_back(text);
}

void BasicTerminal::on_input(std::function<void(std::string_view)> callback) {
    m_input_callback = std::move(callback);
}

void BasicTerminal::request_ai_explanation(std::string_view command_context) {
    (void)command_context;
    write_output("[AI is thinking...]");
}

void BasicTerminal::request_ai_suggestion(std::string_view intent) {
    (void)intent;
    write_output("[AI Suggestion pending...]");
}

void BasicTerminal::launch() {
    desktop::WindowGeometry geom{100, 100, 600, 400};
    m_window_id = m_wm->create_window("Terminal", geom);
    
    m_surface = m_compositor->create_surface(geom.width, geom.height);
    m_wm->set_window_surface(m_window_id, m_surface);
}

void BasicTerminal::update_frame() {
    if (m_window_id == 0) return;
    
    // Clear surface
    {
        auto map = m_surface->map_pixels();
        std::fill(map.begin(), map.end(), static_cast<uint8_t>(0));
        m_surface->unmap_pixels();
    }
    
    // Draw background
    m_toolkit->draw_panel(m_surface, 0, 0, 600, 400);
    
    // Draw title bar
    m_toolkit->draw_rect(m_surface, 0, 0, 600, 30, desktop::Color{50, 50, 50, 255});
    m_toolkit->draw_text(m_surface, 10, 10, "Terminal", desktop::Color{255, 255, 255, 255});
    
    // Draw close button
    m_toolkit->draw_button(m_surface, 560, 5, 30, 20, "X");
    
    // Draw text history
    int32_t y_offset = 40;
    for (const auto& line : m_history) {
        m_toolkit->draw_text(m_surface, 10, y_offset, line, desktop::Color{200, 200, 200, 255});
        y_offset += 20;
    }
}

} // namespace vynexos::apps
