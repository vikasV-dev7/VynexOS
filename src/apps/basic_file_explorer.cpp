#include "basic_file_explorer.hpp"
#include "vynexos/hal/input_driver.hpp"
#include <algorithm>

namespace vynexos::apps {

// -----------------------------------------------------------------------------
// DI-Compliant Phase 4.1 Constructor
// -----------------------------------------------------------------------------
BasicFileExplorer::BasicFileExplorer(std::shared_ptr<core::IVirtualFileSystem> vfs,
                                     std::shared_ptr<desktop::IWidgetToolkit> toolkit)
    : m_vfs(std::move(vfs)), m_toolkit(std::move(toolkit)), m_current_path("/") {}


// IApplication Implementation
bool BasicFileExplorer::initialize() {
    if (!m_vfs || !m_toolkit) return false;
    return refresh_current_directory();
}

void BasicFileExplorer::start() {}
void BasicFileExplorer::update(double) {}

bool BasicFileExplorer::handle_event(const vynexos::core::Event& event) {
    if (event.topic != "HAL_INPUT_MOUSE") return false;
    
    if (event.payload.type() == typeid(hal::MouseEvent)) {
        auto mouse_ev = std::any_cast<hal::MouseEvent>(event.payload);
        
        // We only process button down (assuming 1 is down)
        if (mouse_ev.button_state != 1) return false;
        
        int32_t x = mouse_ev.x;
        int32_t y = mouse_ev.y;
        
        // "Up" button bounds: 10, 40, 40, 30
        if (x >= 10 && x <= 50 && y >= 40 && y <= 70) {
            navigate_up();
            return true;
        }
        
        // List entries: starts at y=80, height=30 per entry
        if (y >= 80) {
            int row = (y - 80) / 30;
            if (row >= 0 && row < static_cast<int>(m_entries.size())) {
                if (m_selected_index.has_value() && m_selected_index.value() == static_cast<size_t>(row)) {
                    // Second click: Activation gesture
                    if (m_entries[row].stat.is_directory) {
                        navigate_to(m_entries[row].full_path);
                    }
                } else {
                    // First click: Select
                    m_selected_index = static_cast<size_t>(row);
                }
                return true;
            } else {
                // Click in empty space
                m_selected_index.reset();
            }
        }
    }
    return false;
}

void BasicFileExplorer::render(vynexos::desktop::ISurface& target_surface) {
    if (!m_toolkit) return;
    
    // Create an aliased shared_ptr to pass to the toolkit safely
    std::shared_ptr<desktop::ISurface> surf_ptr(&target_surface, [](desktop::ISurface*){});
    
    uint32_t width = surf_ptr->width();
    uint32_t height = surf_ptr->height();
    
    // Draw background
    m_toolkit->draw_panel(surf_ptr, 0, 0, width, height);
    
    // Top bar (Title)
    m_toolkit->draw_rect(surf_ptr, 0, 0, width, 30, desktop::Color{60, 60, 80, 255});
    std::string title = "File Explorer - " + m_current_path.as_string();
    m_toolkit->draw_text(surf_ptr, 10, 10, title, desktop::Color{255, 255, 255, 255});
    
    // "Up" button
    m_toolkit->draw_button(surf_ptr, 10, 40, 40, 30, "Up");
    
    // List
    int32_t list_y = 80;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_selected_index.has_value() && m_selected_index.value() == i) {
            // Highlight selected row
            m_toolkit->draw_rect(surf_ptr, 10, list_y, width - 20, 30, desktop::Color{100, 100, 180, 255});
        }
        
        const auto& entry = m_entries[i];
        std::string prefix = entry.stat.is_directory ? "[DIR] " : "[FILE] ";
        std::string label = prefix + entry.display_name;
        
        m_toolkit->draw_text(surf_ptr, 20, list_y + 10, label, desktop::Color{200, 200, 200, 255});
        list_y += 30;
    }
    
    // Status bar
    std::string status = "Items: " + std::to_string(m_entries.size());
    m_toolkit->draw_rect(surf_ptr, 0, height - 20, width, 20, desktop::Color{40, 40, 40, 255});
    m_toolkit->draw_text(surf_ptr, 10, height - 15, status, desktop::Color{150, 150, 150, 255});
}

void BasicFileExplorer::shutdown() {}

std::string BasicFileExplorer::get_name() const noexcept {
    return "BasicFileExplorer";
}

bool BasicFileExplorer::refresh_current_directory() {
    if (!m_vfs) return false;
    
    auto files = m_vfs->list_directory(m_current_path.as_string());
    m_entries.clear();
    m_selected_index.reset();
    
    for (const auto& name : files) {
        core::VirtualPath full_path = m_current_path.append(name);
        
        auto stat_res = m_vfs->stat(full_path.as_string());
        if (stat_res) {
            DirectoryEntry entry;
            entry.display_name = name;
            entry.full_path = full_path;
            entry.stat = *stat_res;
            m_entries.push_back(entry);
        }
    }
    
    // Sort directories first, then alphabetical
    std::sort(m_entries.begin(), m_entries.end(), [](const DirectoryEntry& a, const DirectoryEntry& b) {
        if (a.stat.is_directory && !b.stat.is_directory) return true;
        if (b.stat.is_directory && !a.stat.is_directory) return false;
        return a.display_name < b.display_name;
    });
    
    return true;
}

bool BasicFileExplorer::navigate_to(const core::VirtualPath& path) {
    if (!m_vfs) return false;
    
    auto stat_res = m_vfs->stat(path.as_string());
    if (!stat_res || !stat_res->is_directory) {
        return false;
    }
    
    m_current_path = path;
    return refresh_current_directory();
}

void BasicFileExplorer::navigate_up() {
    navigate_to(m_current_path.parent());
}

} // namespace vynexos::apps
