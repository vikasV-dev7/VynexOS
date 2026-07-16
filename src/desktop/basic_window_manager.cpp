#include "basic_window_manager.hpp"
#include "vynexos/hal/input_driver.hpp"

namespace vynexos::desktop {

BasicWindowManager::BasicWindowManager(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger)
    : m_event_bus(std::move(event_bus)), m_logger(std::move(logger)) {
    
    // Subscribe to HAL input events
    m_event_bus->subscribe("HAL_INPUT_MOUSE", [this](std::shared_ptr<const core::Event> event) {
        this->handle_mouse_event(event);
    });
}

void BasicWindowManager::handle_mouse_event(std::shared_ptr<const core::Event> event) {
    if (event->payload.type() != typeid(hal::MouseEvent)) return;
    
    auto mouse_ev = std::any_cast<hal::MouseEvent>(event->payload);
    if (mouse_ev.button_state == 0) return; // Only care about clicks for focus routing
    
    std::lock_guard lock(m_mutex);
    // Iterate Z-order from top to bottom
    for (auto it = m_z_order.rbegin(); it != m_z_order.rend(); ++it) {
        uint32_t win_id = *it;
        const auto& geom = m_windows[win_id].geometry;
        
        if (mouse_ev.x >= geom.x && mouse_ev.x <= (geom.x + static_cast<int32_t>(geom.width)) &&
            mouse_ev.y >= geom.y && mouse_ev.y <= (geom.y + static_cast<int32_t>(geom.height))) {
            
            if (m_focused_window != win_id) {
                m_logger->info("BasicWindowManager: Mouse click at {},{} routed focus to window [ID: {}]", mouse_ev.x, mouse_ev.y, win_id);
                m_z_order.erase(std::next(it).base());
                m_z_order.push_back(win_id);
                m_focused_window = win_id;
            }
            return; // Handled
        }
    }
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
        if (m_focused_window == window_id) {
            m_focused_window = m_z_order.empty() ? 0 : m_z_order.back();
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

std::vector<uint32_t> BasicWindowManager::get_windows_z_ordered() const {
    std::lock_guard lock(m_mutex);
    return m_z_order;
}

} // namespace vynexos::desktop
