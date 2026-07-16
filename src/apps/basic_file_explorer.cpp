#include "basic_file_explorer.hpp"

namespace vynexos::apps {

BasicFileExplorer::BasicFileExplorer(std::shared_ptr<desktop::IWindowManager> wm,
                                     std::shared_ptr<desktop::ICompositor> compositor,
                                     std::shared_ptr<desktop::IWidgetToolkit> toolkit)
    : m_wm(std::move(wm)), m_compositor(std::move(compositor)), m_toolkit(std::move(toolkit)) {}

std::vector<FileNode> BasicFileExplorer::list_directory(std::string_view path) {
    (void)path;
    return {
        {"sys", "/sys", true, 0},
        {"usr", "/usr", true, 0},
        {"home", "/home", true, 0}
    };
}

void BasicFileExplorer::semantic_search_async(std::string_view query, std::function<void(std::vector<FileNode>)> callback) {
    (void)query;
    // Stub
    callback({});
}

void BasicFileExplorer::launch() {
    desktop::WindowGeometry geom{200, 150, 600, 450};
    m_window_id = m_wm->create_window("File Explorer", geom);
    
    m_buffer.id = m_window_id;
    m_buffer.x = geom.x;
    m_buffer.y = geom.y;
    m_buffer.width = geom.width;
    m_buffer.height = geom.height;
    m_buffer.pixels.resize(geom.width * geom.height * 4, static_cast<uint8_t>(255)); // White background
}

void BasicFileExplorer::update_frame() {
    if (m_window_id == 0) return;
    
    // Clear to white
    std::fill(m_buffer.pixels.begin(), m_buffer.pixels.end(), static_cast<uint8_t>(255));
    
    // Title bar
    m_toolkit->draw_panel(m_buffer, 0, 0, m_buffer.width, 30);
    m_toolkit->draw_text(m_buffer, 10, 8, "File Explorer - /", desktop::Color{255, 255, 255, 255});
    
    // Sidebar
    m_toolkit->draw_panel(m_buffer, 0, 30, 150, m_buffer.height - 30);
    m_toolkit->draw_text(m_buffer, 10, 40, "Home", desktop::Color{200, 200, 200, 255});
    m_toolkit->draw_text(m_buffer, 10, 65, "System", desktop::Color{200, 200, 200, 255});
    
    // Files mock
    m_toolkit->draw_button(m_buffer, 170, 50, 80, 80, "sys");
    m_toolkit->draw_button(m_buffer, 270, 50, 80, 80, "usr");
    m_toolkit->draw_button(m_buffer, 370, 50, 80, 80, "home");
    
    m_compositor->submit_buffer(m_buffer);
}

} // namespace vynexos::apps
