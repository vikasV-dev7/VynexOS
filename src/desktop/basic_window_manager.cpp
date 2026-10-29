#include "basic_window_manager.hpp"
#include "software_surface.hpp"
#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/decoration_layout.hpp"
#include "vynexos/hal/input_driver.hpp"
#include <algorithm>

namespace vynexos::desktop {

static bool point_in_rect(int32_t x, int32_t y, const Rect& rect) {
    return x >= rect.x && x < rect.x + static_cast<int32_t>(rect.width) &&
           y >= rect.y && y < rect.y + static_cast<int32_t>(rect.height);
}

BasicWindowManager::BasicWindowManager(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger)
    : m_event_bus(std::move(event_bus)), m_logger(std::move(logger)) {
}

std::optional<uint32_t> BasicWindowManager::get_window_at(int32_t x, int32_t y) const {
    std::lock_guard lock(m_mutex);
    
    // Iterate Z-order from top to bottom
    for (auto it = m_z_order.rbegin(); it != m_z_order.rend(); ++it) {
        uint32_t win_id = *it;
        const auto& win = m_windows.at(win_id);
        
        if (!win.visible || win.state == WindowState::Minimized) {
            continue;
        }
        
        Rect bounds = {
            win.geometry.x - static_cast<int32_t>(win.margins.left),
            win.geometry.y - static_cast<int32_t>(win.margins.top),
            win.geometry.width + static_cast<uint32_t>(win.margins.left + win.margins.right),
            win.geometry.height + static_cast<uint32_t>(win.margins.top + win.margins.bottom)
        };
        
        if (x >= bounds.x && x <= (bounds.x + static_cast<int32_t>(bounds.width)) &&
            y >= bounds.y && y <= (bounds.y + static_cast<int32_t>(bounds.height))) {
            return win_id;
        }
    }
    
    return std::nullopt;
}

std::optional<HitTestResult> BasicWindowManager::get_resize_handle_at(int32_t x, int32_t y) const {
    std::lock_guard lock(m_mutex);
    
    for (auto it = m_z_order.rbegin(); it != m_z_order.rend(); ++it) {
        uint32_t win_id = *it;
        const auto& win = m_windows.at(win_id);
        
        if (!win.visible || win.state == WindowState::Minimized) {
            continue;
        }
        
        Rect bounds = {
            win.geometry.x - static_cast<int32_t>(win.margins.left),
            win.geometry.y - static_cast<int32_t>(win.margins.top),
            win.geometry.width + static_cast<uint32_t>(win.margins.left + win.margins.right),
            win.geometry.height + static_cast<uint32_t>(win.margins.top + win.margins.bottom)
        };
        
        WindowFrameMetrics metrics;
        metrics.decorated_bounds = bounds;
        metrics.client_bounds = {win.geometry.x, win.geometry.y, win.geometry.width, win.geometry.height};
        metrics.margins = win.margins;
        
        DecorationLayout layout = calculate_decoration_layout(metrics);
        
        int32_t rel_x = x - bounds.x;
        int32_t rel_y = y - bounds.y;
        
        HitTestResult result{win_id, ResizeEdge::None};
        
        if (point_in_rect(rel_x, rel_y, layout.top_left_resize)) result.edge = ResizeEdge::TopLeft;
        else if (point_in_rect(rel_x, rel_y, layout.top_right_resize)) result.edge = ResizeEdge::TopRight;
        else if (point_in_rect(rel_x, rel_y, layout.bottom_left_resize)) result.edge = ResizeEdge::BottomLeft;
        else if (point_in_rect(rel_x, rel_y, layout.bottom_right_resize)) result.edge = ResizeEdge::BottomRight;
        else if (point_in_rect(rel_x, rel_y, layout.top_border)) result.edge = ResizeEdge::Top;
        else if (point_in_rect(rel_x, rel_y, layout.bottom_border)) result.edge = ResizeEdge::Bottom;
        else if (point_in_rect(rel_x, rel_y, layout.left_border)) result.edge = ResizeEdge::Left;
        else if (point_in_rect(rel_x, rel_y, layout.right_border)) result.edge = ResizeEdge::Right;
        
        if (result.edge != ResizeEdge::None) return result;
        
        // Return nullopt if clicked inside client but not on a border. (Note: get_window_at already does initial rect checks)
    }
    
    return std::nullopt;
}

DecorationHitRegion BasicWindowManager::get_decoration_hit_region(uint32_t window_id, int32_t x, int32_t y) const {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        const auto& win = it->second;
        if (!win.visible || win.state == WindowState::Minimized) return DecorationHitRegion::None;
        
        Rect bounds = {
            win.geometry.x - static_cast<int32_t>(win.margins.left),
            win.geometry.y - static_cast<int32_t>(win.margins.top),
            win.geometry.width + static_cast<uint32_t>(win.margins.left + win.margins.right),
            win.geometry.height + static_cast<uint32_t>(win.margins.top + win.margins.bottom)
        };
        
        // Quick bounds check
        if (x < bounds.x || x >= bounds.x + static_cast<int32_t>(bounds.width) ||
            y < bounds.y || y >= bounds.y + static_cast<int32_t>(bounds.height)) {
            return DecorationHitRegion::None;
        }
        
        WindowFrameMetrics metrics;
        metrics.decorated_bounds = bounds;
        metrics.client_bounds = {win.geometry.x, win.geometry.y, win.geometry.width, win.geometry.height};
        metrics.margins = win.margins;
        
        DecorationLayout layout = calculate_decoration_layout(metrics);
        
        int32_t rel_x = x - bounds.x;
        int32_t rel_y = y - bounds.y;
        
        if (point_in_rect(rel_x, rel_y, layout.close_button)) return DecorationHitRegion::CloseButton;
        if (point_in_rect(rel_x, rel_y, layout.maximize_button)) return DecorationHitRegion::MaximizeButton;
        if (point_in_rect(rel_x, rel_y, layout.minimize_button)) return DecorationHitRegion::MinimizeButton;
        
        // Borders
        if (point_in_rect(rel_x, rel_y, layout.top_border) || 
            point_in_rect(rel_x, rel_y, layout.bottom_border) || 
            point_in_rect(rel_x, rel_y, layout.left_border) || 
            point_in_rect(rel_x, rel_y, layout.right_border)) {
            return DecorationHitRegion::Border;
        }
        
        if (point_in_rect(rel_x, rel_y, layout.title_bar)) return DecorationHitRegion::TitleBar;
    }
    
    return DecorationHitRegion::None;
}

bool BasicWindowManager::is_window_visible(uint32_t window_id) const {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        return it->second.visible && it->second.state == WindowState::Normal;
    }
    return false;
}

std::string BasicWindowManager::get_window_title(uint32_t window_id) const {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        return it->second.title;
    }
    return "";
}

WindowMargins BasicWindowManager::get_window_margins(uint32_t window_id) const {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        return it->second.margins;
    }
    return WindowMargins{0, 0, 0, 0};
}

Rect BasicWindowManager::get_decorated_window_bounds(uint32_t window_id) const {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        const auto& win = it->second;
        return {
            win.geometry.x - static_cast<int32_t>(win.margins.left),
            win.geometry.y - static_cast<int32_t>(win.margins.top),
            win.geometry.width + static_cast<uint32_t>(win.margins.left + win.margins.right),
            win.geometry.height + static_cast<uint32_t>(win.margins.top + win.margins.bottom)
        };
    }
    return {0, 0, 0, 0};
}

std::optional<WindowGeometry> BasicWindowManager::get_window_geometry(uint32_t window_id) const {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        return it->second.geometry;
    }
    return std::nullopt;
}

uint32_t BasicWindowManager::create_window(std::string_view title, WindowGeometry geometry) {
    std::lock_guard lock(m_mutex);
    uint32_t id = m_next_id++;
    
    WindowRecord record;
    record.title = std::string(title);
    record.geometry = geometry;
    record.visible = true;
    record.state = WindowState::Normal;
    
    m_windows[id] = std::move(record);
    m_z_order.push_back(id);
    m_creation_order.push_back(id);
    m_logger->info("BasicWindowManager: Created window '{}' with ID {}", title, id);
    return id;
}

void BasicWindowManager::destroy_window(uint32_t window_id) {
    std::lock_guard lock(m_mutex);
    if (m_windows.erase(window_id)) {
        m_z_order.erase(std::remove(m_z_order.begin(), m_z_order.end(), window_id), m_z_order.end());
        m_creation_order.erase(std::remove(m_creation_order.begin(), m_creation_order.end(), window_id), m_creation_order.end());
        if (m_focused_window == window_id) {
            m_focused_window = m_z_order.empty() ? std::nullopt : std::make_optional(m_z_order.back());
        }
        m_logger->info("BasicWindowManager: Destroyed window ID {}", window_id);
    }
}

void BasicWindowManager::focus_window(uint32_t window_id) {
    std::lock_guard lock(m_mutex);
    if (m_windows.contains(window_id)) {
        auto it = std::find(m_z_order.begin(), m_z_order.end(), window_id);
        if (it != m_z_order.end()) {
            m_z_order.erase(it);
            m_z_order.push_back(window_id); // Move to top (back of vector)
        }
        m_focused_window = window_id;
        m_logger->info("BasicWindowManager: Focused window [ID: {}]", window_id);
    }
}

void BasicWindowManager::move_window(uint32_t window_id, int32_t x, int32_t y) {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        it->second.geometry.x = x;
        it->second.geometry.y = y;
        m_logger->trace("BasicWindowManager: Moved window [ID: {}] to {},{}", window_id, x, y);
    }
}

void BasicWindowManager::resize_window(uint32_t window_id, uint32_t width, uint32_t height) {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        it->second.geometry.width = std::max(width, it->second.min_width);
        it->second.geometry.height = std::max(height, it->second.min_height);
        m_logger->trace("BasicWindowManager: Resized window [ID: {}] to {}x{}", window_id, width, height);
    }
}

void BasicWindowManager::set_window_geometry(uint32_t window_id, const WindowGeometry& geometry) {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        it->second.geometry.x = geometry.x;
        it->second.geometry.y = geometry.y;
        it->second.geometry.width = std::max(geometry.width, it->second.min_width);
        it->second.geometry.height = std::max(geometry.height, it->second.min_height);
        m_logger->trace("BasicWindowManager: Set geometry window [ID: {}] to {},{} {}x{}", 
                        window_id, geometry.x, geometry.y, geometry.width, geometry.height);
    }
}

void BasicWindowManager::set_work_area(const WindowGeometry& area) {
    std::lock_guard lock(m_mutex);
    m_work_area = area;
    m_logger->info("BasicWindowManager: Work area updated to {},{} {}x{}", area.x, area.y, area.width, area.height);
}

WindowGeometry BasicWindowManager::get_work_area() const {
    std::lock_guard lock(m_mutex);
    return m_work_area;
}

void BasicWindowManager::set_window_surface(uint32_t window_id, std::shared_ptr<ISurface> surface) {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        it->second.surface = std::move(surface);
    }
}

void BasicWindowManager::set_window_decoration(uint32_t window_id, std::shared_ptr<ISurface> decoration, WindowMargins margins) {
    std::lock_guard lock(m_mutex);
    if (m_windows.contains(window_id)) {
        m_windows[window_id].decoration_surface = std::move(decoration);
        m_windows[window_id].margins = margins;
    }
}

void BasicWindowManager::set_window_visibility(uint32_t window_id, bool visible) {
    std::lock_guard lock(m_mutex);
    if (m_windows.contains(window_id)) {
        m_windows[window_id].visible = visible;
    }
}

std::vector<uint32_t> BasicWindowManager::get_windows_z_ordered() const {
    std::lock_guard lock(m_mutex);
    return m_z_order;
}

std::vector<uint32_t> BasicWindowManager::get_visible_windows_z_ordered() const {
    std::lock_guard lock(m_mutex);
    std::vector<uint32_t> visible;
    for (uint32_t id : m_z_order) {
        if (m_windows.at(id).visible && m_windows.at(id).state != WindowState::Minimized) {
            visible.push_back(id);
        }
    }
    return visible;
}

std::vector<WindowSnapshot> BasicWindowManager::get_window_snapshots() const {
    std::lock_guard lock(m_mutex);
    std::vector<WindowSnapshot> snapshots;
    snapshots.reserve(m_creation_order.size());
    for (uint32_t id : m_creation_order) {
        if (auto it = m_windows.find(id); it != m_windows.end()) {
            snapshots.push_back({id, it->second.title, it->second.state, it->second.visible});
        }
    }
    return snapshots;
}

std::optional<uint32_t> BasicWindowManager::get_active_window() const {
    std::lock_guard lock(m_mutex);
    return m_focused_window;
}

void BasicWindowManager::set_window_state(uint32_t window_id, WindowState state) {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        auto& win = it->second;
        
        if (win.state != state) {
            win.previous_state = win.state;
        }
        
        if (win.state == WindowState::Normal && state == WindowState::Maximized) {
            win.restore_geometry = win.geometry;
            win.geometry.x = m_work_area.x + static_cast<int32_t>(win.margins.left);
            win.geometry.y = m_work_area.y + static_cast<int32_t>(win.margins.top);
            win.geometry.width = m_work_area.width > static_cast<uint32_t>(win.margins.left + win.margins.right) ? m_work_area.width - static_cast<uint32_t>(win.margins.left + win.margins.right) : 0;
            win.geometry.height = m_work_area.height > static_cast<uint32_t>(win.margins.top + win.margins.bottom) ? m_work_area.height - static_cast<uint32_t>(win.margins.top + win.margins.bottom) : 0;
        } else if (win.state == WindowState::Maximized && state == WindowState::Normal) {
            // Maximized -> Normal: Restore previous geometry
            if (win.restore_geometry.has_value()) {
                win.geometry = win.restore_geometry.value();
            }
        } else if (win.state == WindowState::Minimized && state == WindowState::Maximized) {
            if (!win.restore_geometry.has_value()) {
                win.restore_geometry = win.geometry;
            }
            win.geometry = m_work_area;
        } else if (win.state == WindowState::Minimized && state == WindowState::Normal) {
            // Minimized -> Normal: Restore previous normal geometry if it exists
            if (win.restore_geometry.has_value()) {
                win.geometry = win.restore_geometry.value();
            }
        } else if (win.state == WindowState::Maximized && state == WindowState::Minimized) {
            // Maximized -> Minimized: Do not alter restore geometry.
        }
        
        win.state = state;
        
        std::string state_str = "Unknown";
        if (state == WindowState::Normal) state_str = "Normal";
        else if (state == WindowState::Minimized) state_str = "Minimized";
        else if (state == WindowState::Maximized) state_str = "Maximized";
        
        m_logger->info("BasicWindowManager: Window [ID: {}] state changed to {}", window_id, state_str);
    }
}

WindowState BasicWindowManager::get_previous_window_state(uint32_t window_id) const {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        return it->second.previous_state;
    }
    return WindowState::Normal;
}

WindowState BasicWindowManager::get_window_state(uint32_t window_id) const {
    std::lock_guard lock(m_mutex);
    if (auto it = m_windows.find(window_id); it != m_windows.end()) {
        return it->second.state;
    }
    return WindowState::Normal; // Default fallback
}


SceneGraph BasicWindowManager::build_scene() const {
    std::lock_guard lock(m_mutex);
    SceneGraph scene;
    scene.reserve(m_z_order.size());
    
    static int frame_count = 0;
    frame_count++;
    
    if (frame_count % 60 == 0) {
        m_logger->info("----------------------------------");
        m_logger->info("Registered Windows:");
        for (const auto& [id, win] : m_windows) {
            auto it = std::find(m_z_order.begin(), m_z_order.end(), id);
            int z_idx = (it != m_z_order.end()) ? static_cast<int>(std::distance(m_z_order.begin(), it)) : -1;
            m_logger->info("Window ID: {}, Title: {}, Visible: {}, Focused: {}, Z-order: {}, Bounds: [{},{},{}x{}], Surface pointer: {}, Opacity: {}",
                id, win.title, win.visible, (m_focused_window.has_value() && m_focused_window.value() == id), z_idx,
                win.geometry.x, win.geometry.y, win.geometry.width, win.geometry.height,
                static_cast<void*>(win.surface.get()), win.opacity);
        }
        m_logger->info("----------------------------------");
        m_logger->info("Generated SceneGraph:");
    }
    
    for (uint32_t id : m_z_order) {
        if (!m_windows.contains(id)) {
            if (frame_count % 60 == 0) m_logger->info("Excluded Window ID: {} - Reason: Missing registration", id);
            continue;
        }
        const auto& win = m_windows.at(id);
        if (!win.surface) {
            if (frame_count % 60 == 0) m_logger->info("Excluded Window ID: {} ({}) - Reason: Null surface", id, win.title);
            continue;
        }
        if (!win.visible) {
            if (frame_count % 60 == 0) m_logger->info("Excluded Window ID: {} ({}) - Reason: Invisible", id, win.title);
            continue;
        }
        if (win.state == WindowState::Minimized) {
            if (frame_count % 60 == 0) m_logger->info("Excluded Window ID: {} ({}) - Reason: Minimized", id, win.title);
            continue;
        }
        if (win.opacity <= 0.0f) {
            if (frame_count % 60 == 0) m_logger->info("Excluded Window ID: {} ({}) - Reason: Zero opacity", id, win.title);
        }
        
        // Push Decoration Layer
        if (win.decoration_surface) {
            RenderLayer dec_layer;
            dec_layer.window_id = id;
            dec_layer.bounds = {
                win.geometry.x - static_cast<int32_t>(win.margins.left),
                win.geometry.y - static_cast<int32_t>(win.margins.top),
                win.geometry.width + static_cast<uint32_t>(win.margins.left + win.margins.right),
                win.geometry.height + static_cast<uint32_t>(win.margins.top + win.margins.bottom)
            };
            dec_layer.clip = {0, 0, 0, 0};
            dec_layer.opacity = win.opacity;
            dec_layer.visible = win.visible;
            dec_layer.surface = win.decoration_surface;
            scene.push_back(std::move(dec_layer));
        }
        
        // Push Content Layer
        if (win.surface) {
            RenderLayer layer;
            layer.window_id = id;
            layer.bounds = {
                win.geometry.x,
                win.geometry.y,
                win.geometry.width,
                win.geometry.height
            };
            layer.clip = {0, 0, 0, 0};
            layer.opacity = win.opacity;
            layer.visible = win.visible;
            layer.surface = win.surface;
            
            if (frame_count % 60 == 0) {
                auto it = std::find(m_z_order.begin(), m_z_order.end(), id);
                int z_idx = (it != m_z_order.end()) ? static_cast<int>(std::distance(m_z_order.begin(), it)) : -1;
                m_logger->info("RenderLayer Content: Window ID: {}, Bounds: [{},{},{}x{}], Clip: [{},{},{}x{}], Opacity: {}, Visible: {}, Surface pointer: {}, Z-order: {}",
                    id, layer.bounds.x, layer.bounds.y, layer.bounds.width, layer.bounds.height,
                    layer.clip.x, layer.clip.y, layer.clip.width, layer.clip.height,
                    layer.opacity, layer.visible, static_cast<void*>(layer.surface.get()), z_idx);
            }
            scene.push_back(std::move(layer));
        }
    }
    
    if (frame_count % 60 == 0) {
        m_logger->info("RenderLayer count: {}", scene.size());
    }
    
    return scene;
}

} // namespace vynexos::desktop
