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
    desktop::WindowGeometry geom{200, 200, 700, 500};
    m_window_id = m_wm->create_window("File Explorer", geom);
    
    m_surface = m_compositor->create_surface(geom.width, geom.height);
    m_wm->set_window_surface(m_window_id, m_surface);
}

void BasicFileExplorer::update_frame() {
    if (m_window_id == 0) return;
    
    // Clear surface
    {
        auto map = m_surface->map_pixels();
        std::fill(map.begin(), map.end(), static_cast<uint8_t>(0));
        m_surface->unmap_pixels();
    }
    
    // Draw background
    m_toolkit->draw_panel(m_surface, 0, 0, 700, 500);
    
    // Draw title bar
    m_toolkit->draw_rect(m_surface, 0, 0, 700, 30, desktop::Color{60, 60, 80, 255});
    m_toolkit->draw_text(m_surface, 10, 10, "File Explorer - /", desktop::Color{255, 255, 255, 255});
    
    // Draw close button
    m_toolkit->draw_button(m_surface, 660, 5, 30, 20, "X");
    
    // Draw some mock folders
    m_toolkit->draw_button(m_surface, 20, 50, 80, 80, "bin");
    m_toolkit->draw_button(m_surface, 120, 50, 80, 80, "etc");
    m_toolkit->draw_button(m_surface, 220, 50, 80, 80, "var");
    m_toolkit->draw_button(m_surface, 320, 50, 80, 80, "usr");
    m_toolkit->draw_button(m_surface, 420, 50, 80, 80, "home");
}

} // namespace vynexos::apps
