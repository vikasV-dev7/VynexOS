#include "basic_taskbar.hpp"
#include "vynexos/desktop/compositor.hpp"
#include <algorithm>

namespace vynexos::desktop {

BasicTaskbar::BasicTaskbar(std::shared_ptr<IWidgetToolkit> toolkit)
    : m_toolkit(std::move(toolkit)) {
}

bool BasicTaskbar::synchronize(const std::vector<TaskbarEntry>& entries) {
    std::lock_guard lock(m_mutex);
    if (m_entries.size() != entries.size()) {
        m_entries = entries;
        return true;
    }
    
    bool changed = false;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (m_entries[i].window_id != entries[i].window_id ||
            m_entries[i].title != entries[i].title ||
            m_entries[i].state != entries[i].state ||
            m_entries[i].is_active != entries[i].is_active) {
            changed = true;
            break;
        }
    }
    
    if (changed) {
        m_entries = entries;
    }
    return changed;
}

void BasicTaskbar::draw(std::shared_ptr<ISurface> surface) {
    std::lock_guard lock(m_mutex);
    if (!surface) return;
    
    int32_t current_x = kStartX;
    
    for (const auto& entry : m_entries) {
        Color bg_color = {80, 80, 80, 255}; // Default inactive
        Color text_color = {200, 200, 200, 255};
        
        if (entry.is_active) {
            bg_color = {50, 100, 200, 255}; // Active blue
            text_color = {255, 255, 255, 255};
        } else if (entry.state == WindowState::Minimized) {
            bg_color = {40, 40, 40, 255}; // Darker gray for minimized
            text_color = {150, 150, 150, 255};
        }
        
        // Custom draw for taskbar buttons
        m_toolkit->draw_panel(surface, current_x, kStartY, kButtonWidth, kButtonHeight);
        
        // We override the panel color to match the state
        auto pixels = surface->map_pixels();
        for (int32_t py = kStartY; py < kStartY + kButtonHeight; ++py) {
            for (int32_t px = current_x; px < current_x + kButtonWidth; ++px) {
                if (px >= 0 && px < static_cast<int32_t>(surface->width()) &&
                    py >= 0 && py < static_cast<int32_t>(surface->height())) {
                    size_t idx = (static_cast<uint32_t>(py) * surface->width() + static_cast<uint32_t>(px)) * 4;
                    pixels[idx] = bg_color.r;
                    pixels[idx+1] = bg_color.g;
                    pixels[idx+2] = bg_color.b;
                    pixels[idx+3] = bg_color.a;
                }
            }
        }
        surface->unmap_pixels();
        
        // Draw the title
        std::string display_title = entry.title;
        if (display_title.length() > 15) {
            display_title = display_title.substr(0, 12) + "...";
        }
        m_toolkit->draw_text(surface, current_x + 10, kStartY + 10, display_title, text_color);
        
        current_x += kButtonWidth + kButtonMargin;
    }
}

std::optional<uint32_t> BasicTaskbar::get_clicked_window(int32_t x, int32_t y) const {
    std::lock_guard lock(m_mutex);
    
    if (y < kStartY || y > kStartY + kButtonHeight) {
        return std::nullopt;
    }
    
    int32_t current_x = kStartX;
    for (const auto& entry : m_entries) {
        if (x >= current_x && x <= current_x + kButtonWidth) {
            return entry.window_id;
        }
        current_x += kButtonWidth + kButtonMargin;
    }
    
    return std::nullopt;
}

} // namespace vynexos::desktop
