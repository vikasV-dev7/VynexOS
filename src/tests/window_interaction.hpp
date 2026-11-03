// Drives the desktop shell the way a user does: mouse events on the bus, in desktop coordinates, aimed at points the
// window manager itself reports (resize zones, title bar, title-bar controls). Shared by the shell-level geometry tests.
#pragma once
#include <cstdint>
#include <memory>

#include "vynexos/core/event_bus.hpp"
#include "vynexos/desktop/decoration_layout.hpp"
#include "vynexos/desktop/window_manager.hpp"
#include "vynexos/hal/input_driver.hpp"

#include "test_check.hpp"

namespace vynex_test {

struct Point {
    int32_t x;
    int32_t y;
};

class Pointer {
public:
    explicit Pointer(vynexos::core::IEventBus& bus) : m_bus(bus) {}

    void send(Point p, uint8_t buttons) {
        auto ev = std::make_shared<vynexos::core::Event>();
        ev->topic = vynexos::hal::kTopicInputMouse;
        ev->payload = vynexos::hal::MouseEvent(p.x, p.y, buttons);
        m_bus.dispatch_immediate(ev);
    }
    void click(Point p) {
        send(p, 1);
        send(p, 0);
    }
    /// Press at `from`, move to `to` with the button held, release there.
    void drag(Point from, Point to) {
        send(from, 1);
        send(to, 1);
        send(to, 0);
    }

private:
    vynexos::core::IEventBus& m_bus;
};

inline vynexos::desktop::DecorationLayout layout_of(const vynexos::desktop::IWindowManager& wm, uint32_t id) {
    const auto c = *wm.get_window_geometry(id);
    return vynexos::desktop::calculate_decoration_layout(vynexos::desktop::make_frame_metrics(
        wm.get_window_frame_style(id), wm.get_decorated_window_bounds(id), {c.x, c.y, c.width, c.height}));
}

/// Centre of a title-bar control, in desktop coordinates.
inline Point control_point(const vynexos::desktop::IWindowManager& wm, uint32_t id, vynexos::desktop::DecorationHitRegion control) {
    using vynexos::desktop::DecorationHitRegion;
    const auto layout = layout_of(wm, id);
    const auto& r = control == DecorationHitRegion::CloseButton      ? layout.close_button
                    : control == DecorationHitRegion::MaximizeButton ? layout.maximize_button
                                                                     : layout.minimize_button;
    const auto b = wm.get_decorated_window_bounds(id);
    const Point p{b.x + r.x + static_cast<int32_t>(r.width / 2), b.y + r.y + static_cast<int32_t>(r.height / 2)};
    CHECK(wm.get_decoration_hit_region(id, p.x, p.y) == control);
    return p;
}

/// A point on the title bar, clear of the controls.
inline Point title_bar_point(const vynexos::desktop::IWindowManager& wm, uint32_t id) {
    const auto layout = layout_of(wm, id);
    const auto b = wm.get_decorated_window_bounds(id);
    const Point p{b.x + layout.title_bar.x + 40, b.y + layout.title_bar.y + static_cast<int32_t>(layout.title_bar.height / 2)};
    CHECK(wm.get_decoration_hit_region(id, p.x, p.y) == vynexos::desktop::DecorationHitRegion::TitleBar);
    return p;
}

/// A point in the middle of the resize zone for `edge` (the zone's centre for corners), checked against the window
/// manager's own hit-testing.
inline Point resize_point(const vynexos::desktop::IWindowManager& wm, uint32_t id, vynexos::desktop::ResizeEdge edge) {
    using vynexos::desktop::ResizeEdge;
    const auto layout = layout_of(wm, id);
    const vynexos::desktop::Rect* r = nullptr;
    switch (edge) {
    case ResizeEdge::Left: r = &layout.left_border; break;
    case ResizeEdge::Right: r = &layout.right_border; break;
    case ResizeEdge::Top: r = &layout.top_border; break;
    case ResizeEdge::Bottom: r = &layout.bottom_border; break;
    case ResizeEdge::TopLeft: r = &layout.top_left_resize; break;
    case ResizeEdge::TopRight: r = &layout.top_right_resize; break;
    case ResizeEdge::BottomLeft: r = &layout.bottom_left_resize; break;
    case ResizeEdge::BottomRight: r = &layout.bottom_right_resize; break;
    default: break;
    }
    CHECK(r != nullptr);
    const auto b = wm.get_decorated_window_bounds(id);
    const Point p{b.x + r->x + static_cast<int32_t>(r->width / 2), b.y + r->y + static_cast<int32_t>(r->height / 2)};
    const auto hit = wm.get_resize_handle_at(p.x, p.y);
    CHECK(hit.has_value() && hit->window_id == id && hit->edge == edge);
    return p;
}

} // namespace vynex_test
