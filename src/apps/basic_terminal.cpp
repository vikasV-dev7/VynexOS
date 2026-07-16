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
    desktop::WindowGeometry geom{400, 200, 700, 400};
    m_window_id = m_wm->create_window("Terminal", geom);
    
    m_buffer.id = m_window_id;
    m_buffer.x = geom.x;
    m_buffer.y = geom.y;
    m_buffer.width = geom.width;
    m_buffer.height = geom.height;
    m_buffer.pixels.resize(geom.width * geom.height * 4, static_cast<uint8_t>(0)); // Black background
}

void BasicTerminal::update_frame() {
    if (m_window_id == 0) return;
    
    // Clear to black
    for (size_t i = 0; i < m_buffer.pixels.size(); i += 4) {
        m_buffer.pixels[i] = 20;
        m_buffer.pixels[i+1] = 20;
        m_buffer.pixels[i+2] = 20;
        m_buffer.pixels[i+3] = 255;
    }
    
    // Title bar
    m_toolkit->draw_panel(m_buffer, 0, 0, m_buffer.width, 25);
    m_toolkit->draw_text(m_buffer, 5, 5, "Terminal", desktop::Color{255, 255, 255, 255});
    
    // Draw text history
    int32_t y_offset = 35;
    for (const auto& line : m_history) {
        m_toolkit->draw_text(m_buffer, 10, y_offset, line, desktop::Color{0, 255, 0, 255});
        y_offset += 20;
    }
    
    m_compositor->submit_buffer(m_buffer);
}

} // namespace vynexos::apps
