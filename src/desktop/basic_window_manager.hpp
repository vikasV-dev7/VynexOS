#pragma once
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/core/logger.hpp"
#include <mutex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

namespace vynexos::desktop {

class BasicWindowManager final : public IWindowManager {
public:
    BasicWindowManager(std::shared_ptr<core::IEventBus> event_bus, std::shared_ptr<core::ILogger> logger);
    ~BasicWindowManager() override = default;

    uint32_t create_window(std::string_view title, WindowGeometry geometry) override;
    void destroy_window(uint32_t window_id) override;
    
    void focus_window(uint32_t window_id) override;
    void move_window(uint32_t window_id, int32_t x, int32_t y) override;
    void resize_window(uint32_t window_id, uint32_t width, uint32_t height) override;
    
    std::vector<uint32_t> get_windows_z_ordered() const override;

private:
    void handle_mouse_event(std::shared_ptr<const core::Event> event);

    struct WindowState {
        std::string title;
        WindowGeometry geometry;
    };

    std::shared_ptr<core::IEventBus> m_event_bus;
    std::shared_ptr<core::ILogger> m_logger;
    mutable std::mutex m_mutex;
    
    uint32_t m_next_id{1};
    std::unordered_map<uint32_t, WindowState> m_windows;
    std::vector<uint32_t> m_z_order; // front is bottom, back is top
    uint32_t m_focused_window{0};
};

} // namespace vynexos::desktop
