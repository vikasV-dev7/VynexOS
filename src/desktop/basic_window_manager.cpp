#include "basic_window_manager.hpp"
#include "software_surface.hpp"
#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/hal/input_driver.hpp"

namespace vynexos::desktop {

BasicWindowManager::BasicWindowManager(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger)
    : m_event_bus(std::move(event_bus)), m_logger(std::move(logger)) {
}

std::optional<uint32_t> BasicWindowManager::get_window_at(int32_t x, int32_t y) const {
    std::lock_guard lock(m_mutex);
    
    // Iterate Z-order from top to bottom
    for (auto it = m_z_order.rbegin(); it != m_z_order.rend(); ++it) {
        uint32_t win_id = *it;
        const auto& win = m_windows.at(win_id);
        
        if (!win.visible) {
            continue;
        }
        
        const auto& geom = win.geometry;
        if (x >= geom.x && x <= (geom.x + static_cast<int32_t>(geom.width)) &&
            y >= geom.y && y <= (geom.y + static_cast<int32_t>(geom.height))) {
            return win_id;
        }
    }
    
    return std::nullopt;
}

uint32_t BasicWindowManager::create_window(std::string_view title, WindowGeometry geometry) {
    std::lock_guard lock(m_mutex);
    uint32_t id = m_next_id++;
    
    m_windows[id] = WindowState{std::string(title), geometry};
    m_z_order.push_back(id);
    m_focused_window = id;
    
    m_logger->info("BasicWindowManager: Created window '{}' [ID: {}] at {},{} ({}x{})", 
                   title, id, geometry.x, geometry.y, geometry.width, geometry.height);
    return id;
}

void BasicWindowManager::destroy_window(uint32_t window_id) {
    std::lock_guard lock(m_mutex);
    if (m_windows.erase(window_id)) {
        auto it = std::find(m_z_order.begin(), m_z_order.end(), window_id);
        if (it != m_z_order.end()) {
            m_z_order.erase(it);
        }
        if (m_focused_window.has_value() && m_focused_window.value() == window_id) {
            m_focused_window = m_z_order.empty() ? std::nullopt : std::make_optional(m_z_order.back());
        }
        m_logger->info("BasicWindowManager: Destroyed window [ID: {}]", window_id);
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
        it->second.geometry.width = width;
        it->second.geometry.height = height;
        m_logger->trace("BasicWindowManager: Resized window [ID: {}] to {}x{}", window_id, width, height);
    }
}

void BasicWindowManager::set_window_surface(uint32_t window_id, std::shared_ptr<ISurface> surface) {
    std::lock_guard lock(m_mutex);
    if (m_windows.contains(window_id)) {
        m_windows[window_id].surface = std::move(surface);
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
                (void*)win.surface.get(), win.opacity);
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
        if (win.opacity <= 0.0f) {
            if (frame_count % 60 == 0) m_logger->info("Excluded Window ID: {} ({}) - Reason: Zero opacity", id, win.title);
        }
        
        RenderLayer layer;
        layer.window_id = id;
        layer.bounds = {win.geometry.x, win.geometry.y, win.geometry.width, win.geometry.height};
        layer.clip = {0, 0, 0, 0}; // 0 implies no clip or full screen in this basic implementation
        layer.opacity = win.opacity;
        layer.visible = win.visible;
        layer.surface = win.surface;
        scene.push_back(std::move(layer));
        
        if (frame_count % 60 == 0) {
            auto it = std::find(m_z_order.begin(), m_z_order.end(), id);
            int z_idx = (it != m_z_order.end()) ? static_cast<int>(std::distance(m_z_order.begin(), it)) : -1;
            m_logger->info("RenderLayer: Window ID: {}, Bounds: [{},{},{}x{}], Clip: [{},{},{}x{}], Opacity: {}, Visible: {}, Surface pointer: {}, Z-order: {}",
                id, layer.bounds.x, layer.bounds.y, layer.bounds.width, layer.bounds.height,
                layer.clip.x, layer.clip.y, layer.clip.width, layer.clip.height,
                layer.opacity, layer.visible, (void*)layer.surface.get(), z_idx);
        }
    }
    
    if (frame_count % 60 == 0) {
        m_logger->info("RenderLayer count: {}", scene.size());
    }
    
    return scene;
}

} // namespace vynexos::desktop
