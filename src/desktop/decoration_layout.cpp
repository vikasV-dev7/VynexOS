#include "vynexos/desktop/decoration_layout.hpp"

namespace vynexos::desktop {

DecorationLayout calculate_decoration_layout(const WindowFrameMetrics& metrics) {
    DecorationLayout layout;
    
    int32_t width = static_cast<int32_t>(metrics.decorated_bounds.width);
    int32_t height = static_cast<int32_t>(metrics.decorated_bounds.height);
    
    // Title bar covers the top margin, spanning full width
    layout.title_bar = {0, 0, metrics.decorated_bounds.width, static_cast<uint32_t>(metrics.margins.top)};
    
    // Client area is the internal content area
    layout.client_area = {
        static_cast<int32_t>(metrics.margins.left),
        static_cast<int32_t>(metrics.margins.top),
        metrics.decorated_bounds.width - static_cast<uint32_t>(metrics.margins.left + metrics.margins.right),
        metrics.decorated_bounds.height - static_cast<uint32_t>(metrics.margins.top + metrics.margins.bottom)
    };
    
    // Buttons (Close, Minimize, Maximize) ordered Right-to-Left
    // Close is rightmost
    int32_t current_x = width - static_cast<int32_t>(metrics.button_margin_right) - static_cast<int32_t>(metrics.button_size);
    int32_t button_y = static_cast<int32_t>(metrics.button_margin_top);
    
    layout.close_button = {current_x, button_y, metrics.button_size, metrics.button_size};
    
    // Minimize is left of Close
    current_x -= static_cast<int32_t>(metrics.button_spacing + metrics.button_size);
    layout.minimize_button = {current_x, button_y, metrics.button_size, metrics.button_size};
    
    // Maximize is left of Minimize
    current_x -= static_cast<int32_t>(metrics.button_spacing + metrics.button_size);
    layout.maximize_button = {current_x, button_y, metrics.button_size, metrics.button_size};
    
    // Borders
    int32_t border_t = static_cast<int32_t>(metrics.border_thickness);
    layout.left_border = {0, static_cast<int32_t>(metrics.margins.top), static_cast<uint32_t>(border_t), metrics.decorated_bounds.height - static_cast<uint32_t>(metrics.margins.top)};
    layout.right_border = {width - border_t, static_cast<int32_t>(metrics.margins.top), static_cast<uint32_t>(border_t), metrics.decorated_bounds.height - static_cast<uint32_t>(metrics.margins.top)};
    layout.top_border = {0, 0, metrics.decorated_bounds.width, static_cast<uint32_t>(border_t)};
    layout.bottom_border = {0, height - border_t, metrics.decorated_bounds.width, static_cast<uint32_t>(border_t)};
    
    // Resize corners (using border_thickness as hit area for corners too, or a bit larger if needed. Standard is border_thickness x border_thickness, or slightly larger like 10x10)
    int32_t corner_size = border_t * 2; // Make corners easier to hit
    layout.top_left_resize = {0, 0, static_cast<uint32_t>(corner_size), static_cast<uint32_t>(corner_size)};
    layout.top_right_resize = {width - corner_size, 0, static_cast<uint32_t>(corner_size), static_cast<uint32_t>(corner_size)};
    layout.bottom_left_resize = {0, height - corner_size, static_cast<uint32_t>(corner_size), static_cast<uint32_t>(corner_size)};
    layout.bottom_right_resize = {width - corner_size, height - corner_size, static_cast<uint32_t>(corner_size), static_cast<uint32_t>(corner_size)};
    
    return layout;
}

} // namespace vynexos::desktop
