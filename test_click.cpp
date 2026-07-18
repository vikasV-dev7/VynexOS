#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>

struct MouseEvent {
    int32_t x, y;
    uint8_t button_state;
};

struct WindowGeometry {
    int32_t x, y;
    uint32_t width, height;
};

struct WindowState {
    std::string title;
    WindowGeometry geometry;
};

int main() {
    std::vector<uint32_t> m_z_order = {1, 2, 3, 4, 5, 6};
    std::vector<WindowState> m_windows = {
        {"", {0,0,0,0}}, // 0 (invalid)
        {"Wallpaper", {0, 0, 1920, 1080}}, // 1
        {"Taskbar", {0, 1040, 1920, 40}}, // 2
        {"Launcher", {10, 640, 300, 400}}, // 3
        {"Terminal", {100, 100, 600, 400}}, // 4
        {"FileExplorer", {200, 150, 600, 450}}, // 5
        {"DemoApp", {300, 200, 600, 400}} // 6
    };
    uint32_t m_focused_window = 6;
    
    // Click at 10, 10 (Desktop background)
    MouseEvent mouse_ev{10, 10, 1};
    
    for (auto it = m_z_order.rbegin(); it != m_z_order.rend(); ++it) {
        uint32_t win_id = *it;
        const auto& geom = m_windows[win_id].geometry;
        
        std::cout << "Checking window " << m_windows[win_id].title << "\n";
        
        if (mouse_ev.x >= geom.x && mouse_ev.x <= (geom.x + static_cast<int32_t>(geom.width)) &&
            mouse_ev.y >= geom.y && mouse_ev.y <= (geom.y + static_cast<int32_t>(geom.height))) {
            
            std::cout << "Matched window " << m_windows[win_id].title << "\n";
            
            if (m_focused_window != win_id) {
                auto fwd_it = std::find(m_z_order.begin(), m_z_order.end(), win_id);
                if (fwd_it != m_z_order.end()) {
                    m_z_order.erase(fwd_it);
                }
                
                m_z_order.push_back(win_id);
                m_focused_window = win_id;
            }
            break; // Handled
        }
    }
    
    std::cout << "Z-order is now: ";
    for (auto id : m_z_order) std::cout << m_windows[id].title << " ";
    std::cout << "\n";
}
