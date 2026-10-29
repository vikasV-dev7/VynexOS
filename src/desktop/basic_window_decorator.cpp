#include "basic_window_decorator.hpp"
#include <algorithm>

namespace vynexos::desktop {

BasicWindowDecorator::BasicWindowDecorator(std::shared_ptr<IWidgetToolkit> toolkit)
    : m_toolkit(std::move(toolkit)) {}

void BasicWindowDecorator::draw_decorations(
    std::shared_ptr<ISurface> surface, 
    const DecorationLayout& layout,
    std::string_view title, 
    bool focused,
    WindowState state,
    const WindowTheme& theme) {
    
    if (!surface || surface->width() == 0 || surface->height() == 0) return;
    
    // Clear the entire surface to transparent first
    {
        auto map = surface->map_pixels();
        std::fill(map.begin(), map.end(), static_cast<uint8_t>(0));
        surface->unmap_pixels();
    }
    
    Color title_bg = focused ? theme.active_title_color : theme.inactive_title_color;
    
    // Draw Title Bar
    m_toolkit->draw_rect(surface, layout.title_bar.x, layout.title_bar.y, layout.title_bar.width, layout.title_bar.height, title_bg);
    
    // Draw Borders
    m_toolkit->draw_rect(surface, layout.left_border.x, layout.left_border.y, layout.left_border.width, layout.left_border.height, theme.border_color);
    m_toolkit->draw_rect(surface, layout.right_border.x, layout.right_border.y, layout.right_border.width, layout.right_border.height, theme.border_color);
    m_toolkit->draw_rect(surface, layout.bottom_border.x, layout.bottom_border.y, layout.bottom_border.width, layout.bottom_border.height, theme.border_color);
    
    // Draw Title Text (approximate center inside title bar)
    m_toolkit->draw_text(surface, layout.title_bar.x + 15, layout.title_bar.y + 10, title, theme.text_color);
    
    // Draw Close Button
    Color close_bg = {200, 50, 50, 255}; // Reddish close button
    m_toolkit->draw_rect(surface, layout.close_button.x, layout.close_button.y, layout.close_button.width, layout.close_button.height, close_bg);
    m_toolkit->draw_text(surface, layout.close_button.x + 10, layout.close_button.y + 5, "X", {255, 255, 255, 255});
    
    // Draw Minimize Button
    Color min_bg = {100, 100, 100, 255}; // Gray minimize button
    m_toolkit->draw_rect(surface, layout.minimize_button.x, layout.minimize_button.y, layout.minimize_button.width, layout.minimize_button.height, min_bg);
    
    // Draw Maximize/Restore Button
    Color max_bg = {50, 150, 50, 255}; // Greenish maximize button
    m_toolkit->draw_rect(surface, layout.maximize_button.x, layout.maximize_button.y, layout.maximize_button.width, layout.maximize_button.height, max_bg);
    
    // Draw inner square for maximize/restore icon
    if (state == WindowState::Maximized) {
        // Restore icon: two overlapping squares
        m_toolkit->draw_rect(surface, layout.maximize_button.x + 10, layout.maximize_button.y + 7, 10, 10, {255, 255, 255, 255});
        m_toolkit->draw_rect(surface, layout.maximize_button.x + 6, layout.maximize_button.y + 11, 10, 10, {200, 200, 200, 255});
    } else {
        // Maximize icon: one square
        m_toolkit->draw_rect(surface, layout.maximize_button.x + 8, layout.maximize_button.y + 8, 14, 14, {255, 255, 255, 255});
    }
    m_toolkit->draw_text(surface, layout.minimize_button.x + 10, layout.minimize_button.y + 5, "_", {255, 255, 255, 255});
}

WindowMargins BasicWindowDecorator::get_margins() const {
    return WindowMargins{40, 5, 5, 5}; // Top: 40px, Bottom: 5px, Left: 5px, Right: 5px
}

} // namespace vynexos::desktop
