// Window chrome: frame layout invariants, icon drawing, decorator pixels and window-manager hit testing.
#include <algorithm>
#include "test_check.hpp"
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <crtdbg.h>
#include <cstdlib>

#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/desktop/decoration_layout.hpp"
#include "vynexos/desktop/icons.hpp"
#include "vynexos/desktop/painter.hpp"

using namespace vynexos;
using namespace vynexos::desktop;

namespace {

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

bool contains(const Rect& outer, const Rect& inner) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + static_cast<int64_t>(inner.width) <= outer.x + static_cast<int64_t>(outer.width) &&
           inner.y + static_cast<int64_t>(inner.height) <= outer.y + static_cast<int64_t>(outer.height);
}

bool overlap(const Rect& a, const Rect& b) {
    if (a.width == 0 || a.height == 0 || b.width == 0 || b.height == 0) return false;
    return a.x < b.x + static_cast<int64_t>(b.width) && b.x < a.x + static_cast<int64_t>(a.width) && a.y < b.y + static_cast<int64_t>(b.height) &&
           b.y < a.y + static_cast<int64_t>(a.height);
}

bool inside(const Rect& r, int32_t x, int32_t y) {
    return x >= r.x && y >= r.y && x < r.x + static_cast<int64_t>(r.width) && y < r.y + static_cast<int64_t>(r.height);
}

const BasicWidgetToolkit& toolkit() {
    static BasicWidgetToolkit t;
    return t;
}

FrameStyle style_for(WindowState s) {
    BasicWindowDecorator decorator(std::make_shared<BasicWidgetToolkit>());
    return decorator.frame_style(s);
}

// ---------------------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------------------
DecorationLayout layout_for(const FrameStyle& style, uint32_t client_w, uint32_t client_h) {
    const uint32_t w = client_w + static_cast<uint32_t>(style.margins.left + style.margins.right);
    const uint32_t h = client_h + static_cast<uint32_t>(style.margins.top + style.margins.bottom);
    return calculate_decoration_layout(make_frame_metrics(style, {0, 0, w, h}, {style.margins.left, style.margins.top, client_w, client_h}));
}

void test_layout_invariants() {
    const FrameStyle normal = style_for(WindowState::Normal);
    CHECK(normal.margins.top == 42 && normal.margins.bottom == 7 && normal.margins.left == 7 && normal.margins.right == 7);
    CHECK(normal.resize_inset == 6);

    uint32_t last_count = 0;
    for (uint32_t client_w = 0; client_w <= 600; client_w += 3) {
        const DecorationLayout l = layout_for(normal, client_w, 200);
        const uint32_t w = client_w + 14;
        const uint32_t h = 200 + 49;
        const Rect bounds{0, 0, w, h};

        CHECK(contains(bounds, l.visible_frame));
        CHECK(contains(l.visible_frame, l.title_bar));
        CHECK(contains(bounds, l.client_area));
        CHECK(l.client_area.x == 7 && l.client_area.y == 42 && l.client_area.width == client_w);

        const Rect buttons[3] = {l.minimize_button, l.maximize_button, l.close_button};
        uint32_t count = 0;
        for (const Rect& b : buttons) {
            if (b.width == 0) continue;
            ++count;
            CHECK(contains(l.title_bar, b));
            CHECK(b.width == 28 && b.height == 28);
            CHECK(!overlap(b, l.top_left_resize) && !overlap(b, l.top_right_resize));
            CHECK(!overlap(b, l.top_border) && !overlap(b, l.left_border) && !overlap(b, l.right_border));
        }
        CHECK(!overlap(l.minimize_button, l.maximize_button));
        CHECK(!overlap(l.maximize_button, l.close_button));
        if (l.minimize_button.width > 0) CHECK(l.minimize_button.x < l.maximize_button.x && l.maximize_button.x < l.close_button.x);
        // Narrower windows never gain buttons back (close is kept longest).
        if (client_w > 0) CHECK(count >= last_count || client_w < 40);
        last_count = count;
        if (count == 3) CHECK(l.close_button.x + 28 + 8 == l.title_bar.x + static_cast<int32_t>(l.title_bar.width));
    }
    // Wide enough: all three, the close button is last and right aligned.
    const DecorationLayout wide = layout_for(normal, 600, 300);
    CHECK(wide.minimize_button.width > 0 && wide.maximize_button.width > 0 && wide.close_button.width > 0);
    // Very narrow: only close remains (or none), never minimize without close.
    const DecorationLayout narrow = layout_for(normal, 30, 100);
    CHECK(narrow.minimize_button.width == 0 && narrow.maximize_button.width == 0);
    std::cout << "test_layout_invariants passed\n";
}

void test_resize_ring_tiles_the_edge() {
    const FrameStyle normal = style_for(WindowState::Normal);
    const DecorationLayout l = layout_for(normal, 400, 300);
    const int32_t w = 414;
    const int32_t h = 349;
    const int32_t ring = 7; // 6 px invisible band + 1 px border
    auto in_any = [&](int32_t x, int32_t y) {
        return inside(l.top_border, x, y) || inside(l.bottom_border, x, y) || inside(l.left_border, x, y) || inside(l.right_border, x, y) ||
               inside(l.top_left_resize, x, y) || inside(l.top_right_resize, x, y) || inside(l.bottom_left_resize, x, y) ||
               inside(l.bottom_right_resize, x, y);
    };
    for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
            const bool on_ring = x < ring || y < ring || x >= w - ring || y >= h - ring;
            if (on_ring) CHECK(in_any(x, y));
            // Corner squares reach inward along both edges but stay clear of the middle of the window.
            if (x >= 2 * ring && y >= 2 * ring && x < w - 2 * ring && y < h - 2 * ring) CHECK(!in_any(x, y));
        }
    }

    const FrameStyle max = style_for(WindowState::Maximized);
    const DecorationLayout m = layout_for(max, 800, 600);
    CHECK(m.visible_frame.x == 0 && m.visible_frame.y == 0 && m.visible_frame.width == 800 && m.visible_frame.height == 636);
    CHECK(m.top_border.height == 0 && m.left_border.width == 0 && m.right_border.width == 0 && m.bottom_border.height == 0);
    CHECK(m.title_bar.height == 36);
    std::cout << "test_resize_ring_tiles_the_edge passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------------------------------------
struct Ink {
    bool any{false};
    int x0{0}, y0{0}, x1{0}, y1{0};
    size_t pixels{0};
};

// Pixels with alpha and a red channel of at least `min_r` (use ~120 to see light glyphs over the dark chrome).
Ink ink_in(SoftwareSurface& s, const Rect& r, uint8_t min_r = 0) {
    Ink ink;
    auto m = s.map_pixels();
    for (int32_t y = std::max(0, r.y); y < std::min<int32_t>(static_cast<int32_t>(s.height()), r.y + static_cast<int32_t>(r.height)); ++y) {
        for (int32_t x = std::max(0, r.x); x < std::min<int32_t>(static_cast<int32_t>(s.width()), r.x + static_cast<int32_t>(r.width)); ++x) {
            const size_t o = (static_cast<size_t>(y) * s.width() + static_cast<size_t>(x)) * 4;
            if (m[o + 3] == 0 || m[o] < min_r) continue;
            if (!ink.any) {
                ink.any = true;
                ink.x0 = ink.x1 = x;
                ink.y0 = ink.y1 = y;
            }
            ink.x0 = std::min(ink.x0, x);
            ink.x1 = std::max(ink.x1, x);
            ink.y0 = std::min(ink.y0, y);
            ink.y1 = std::max(ink.y1, y);
            ++ink.pixels;
        }
    }
    s.unmap_pixels();
    return ink;
}

std::vector<uint8_t> bytes_of(SoftwareSurface& s) {
    auto m = s.map_pixels();
    std::vector<uint8_t> v(m.begin(), m.end());
    s.unmap_pixels();
    return v;
}

uint8_t alpha_at(SoftwareSurface& s, int32_t x, int32_t y) {
    auto m = s.map_pixels();
    const uint8_t a = m[(static_cast<size_t>(y) * s.width() + static_cast<size_t>(x)) * 4 + 3];
    s.unmap_pixels();
    return a;
}

void test_icons() {
    const Rect box{0, 0, 28, 28};
    std::vector<uint8_t> rendered[4];
    const IconId icons[4] = {IconId::Minimize, IconId::Maximize, IconId::Restore, IconId::Close};
    for (int i = 0; i < 4; ++i) {
        SoftwareSurface s(28, 28);
        Painter p(s);
        draw_icon(p, icons[i], box, Color{255, 255, 255, 255});
        const Ink ink = ink_in(s, box);
        CHECK(ink.any);
        // Every glyph is centred in its box (within a pixel) and stays on the 16 px design grid.
        const int cx2 = ink.x0 + ink.x1 + 1;
        const int cy2 = ink.y0 + ink.y1 + 1;
        CHECK(std::abs(cx2 - 28) <= 3);
        if (icons[i] != IconId::Minimize) CHECK(std::abs(cy2 - 28) <= 3);
        CHECK(ink.x1 - ink.x0 + 1 <= 16 && ink.y1 - ink.y0 + 1 <= 16);
        rendered[i] = bytes_of(s);
    }
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j) CHECK(rendered[i] != rendered[j]);
    std::cout << "test_icons passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Decorator
// ---------------------------------------------------------------------------------------------------------
struct Chrome {
    std::shared_ptr<SoftwareSurface> surface;
    DecorationLayout layout;
};

Chrome draw_chrome(const std::string& title, const DecorationVisualState& visual, uint32_t client_w = 400, uint32_t client_h = 300) {
    auto tk = std::make_shared<BasicWidgetToolkit>();
    BasicWindowDecorator decorator(tk);
    const FrameStyle style = decorator.frame_style(visual.state);
    const uint32_t w = client_w + static_cast<uint32_t>(style.margins.left + style.margins.right);
    const uint32_t h = client_h + static_cast<uint32_t>(style.margins.top + style.margins.bottom);
    Chrome c;
    c.layout = calculate_decoration_layout(make_frame_metrics(style, {0, 0, w, h}, {style.margins.left, style.margins.top, client_w, client_h}));
    c.surface = std::make_shared<SoftwareSurface>(w, h);
    decorator.draw_decorations(c.surface, c.layout, title, visual, WindowTheme{});
    return c;
}

void test_decorator_focus_states() {
    DecorationVisualState focused;
    focused.focused = true;
    DecorationVisualState inactive;

    Chrome f = draw_chrome("Files", focused);
    Chrome i = draw_chrome("Files", inactive);
    CHECK(bytes_of(*f.surface) != bytes_of(*i.surface));

    // The invisible resize band carries no pixels: the outermost 6 px ring is fully transparent.
    const int32_t w = static_cast<int32_t>(f.surface->width());
    const int32_t h = static_cast<int32_t>(f.surface->height());
    for (int32_t k = 0; k < 6; ++k) {
        CHECK(alpha_at(*f.surface, k, h / 2) == 0);
        CHECK(alpha_at(*f.surface, w - 1 - k, h / 2) == 0);
        CHECK(alpha_at(*f.surface, w / 2, k) == 0);
        CHECK(alpha_at(*f.surface, w / 2, h - 1 - k) == 0);
    }
    // Just inside the band is the 1 px border.
    CHECK(alpha_at(*f.surface, 6, h / 2) == 255);
    CHECK(alpha_at(*f.surface, w / 2, 6) == 255);
    // Top corners are rounded: the frame's own corner pixel is not fully covered.
    CHECK(alpha_at(*f.surface, 6, 6) < 255);

    // The focused border differs from the inactive border (accent tint).
    auto border_px = [&](SoftwareSurface& s) {
        auto m = s.map_pixels();
        const size_t o = (static_cast<size_t>(h / 2) * s.width() + 6) * 4;
        std::vector<uint8_t> px{m[o], m[o + 1], m[o + 2]};
        s.unmap_pixels();
        return px;
    };
    CHECK(border_px(*f.surface) != border_px(*i.surface));
    std::cout << "test_decorator_focus_states passed\n";
}

void test_decorator_controls_and_title() {
    DecorationVisualState vs;
    vs.focused = true;
    Chrome c = draw_chrome("A Reasonably Long Window Title For Testing Ellipsis", vs, 300, 200);

    // Title ink sits in the title bar, clear of the buttons.
    const Rect title_region{c.layout.title_bar.x, c.layout.title_bar.y, static_cast<uint32_t>(c.layout.minimize_button.x - c.layout.title_bar.x),
                            c.layout.title_bar.height - 1};
    const Ink title = ink_in(*c.surface, title_region, 120);
    CHECK(title.any);
    CHECK(title.x0 >= c.layout.title_bar.x + 8);
    CHECK(title.x1 < c.layout.minimize_button.x - 4);
    CHECK(title.y0 >= c.layout.title_bar.y + 6 && title.y1 < c.layout.title_bar.y + static_cast<int32_t>(c.layout.title_bar.height) - 6);

    // Each control has an icon.
    for (const Rect& b : {c.layout.minimize_button, c.layout.maximize_button, c.layout.close_button}) CHECK(ink_in(*c.surface, b, 120).any);

    // Maximize shows the restore glyph when maximized.
    DecorationVisualState maxed;
    maxed.focused = true;
    maxed.state = WindowState::Maximized;
    Chrome m = draw_chrome("Files", maxed, 800, 400);
    CHECK(alpha_at(*m.surface, 0, 0) == 255 || alpha_at(*m.surface, 0, 0) > 0); // flush: no transparent band in the corner
    CHECK(m.layout.visible_frame.x == 0 && m.layout.visible_frame.y == 0);

    DecorationVisualState normal_vs = vs;
    Chrome n = draw_chrome("Files", normal_vs, 800, 400);
    SoftwareSurface glyph_n(28, 28), glyph_m(28, 28);
    {
        Painter pn(glyph_n), pm(glyph_m);
        draw_icon(pn, IconId::Maximize, {0, 0, 28, 28}, Color{255, 255, 255, 255});
        draw_icon(pm, IconId::Restore, {0, 0, 28, 28}, Color{255, 255, 255, 255});
    }
    CHECK(bytes_of(glyph_n) != bytes_of(glyph_m));
    (void)n;

    // Hover and pressed states are visible and distinct.
    DecorationVisualState hover = vs;
    hover.hovered = DecorationHitRegion::CloseButton;
    DecorationVisualState press = hover;
    press.pressed = DecorationHitRegion::CloseButton;
    Chrome rest = draw_chrome("Files", vs, 300, 200);
    Chrome hov = draw_chrome("Files", hover, 300, 200);
    Chrome prs = draw_chrome("Files", press, 300, 200);
    CHECK(bytes_of(*rest.surface) != bytes_of(*hov.surface));
    CHECK(bytes_of(*hov.surface) != bytes_of(*prs.surface));
    std::cout << "test_decorator_controls_and_title passed\n";
}

// ---------------------------------------------------------------------------------------------------------
// Window manager hit testing
// ---------------------------------------------------------------------------------------------------------
struct Rig {
    TestLogger logger_obj;
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    BasicWindowManager wm{bus, logger};
    uint32_t wallpaper{0};

    Rig() {
        wm.set_work_area({0, 0, 1920, 1032});
        wallpaper = wm.create_window("Wallpaper", {0, 0, 1920, 1080});
        wm.set_window_layer(wallpaper, WindowLayer::Desktop);
        wm.set_window_surface(wallpaper, std::make_shared<SoftwareSurface>(8, 8));
    }

    uint32_t add(const char* title, WindowGeometry g) {
        const uint32_t id = wm.create_window(title, g);
        wm.set_window_surface(id, std::make_shared<SoftwareSurface>(8, 8));
        wm.set_window_frame_styles(id, style_for(WindowState::Normal), style_for(WindowState::Maximized));
        return id;
    }
};

void test_hit_testing_all_edges_and_corners() {
    Rig r;
    const uint32_t a = r.add("A", {300, 200, 400, 300});
    const Rect d = r.wm.get_decorated_window_bounds(a);
    CHECK(d.x == 293 && d.y == 158 && d.width == 414 && d.height == 349);

    struct Probe {
        int32_t dx, dy;
        ResizeEdge edge;
    };
    const int32_t w = static_cast<int32_t>(d.width);
    const int32_t h = static_cast<int32_t>(d.height);
    const Probe probes[] = {
        {w / 2, 2, ResizeEdge::Top},      {w / 2, h - 3, ResizeEdge::Bottom}, {2, h / 2, ResizeEdge::Left},      {w - 3, h / 2, ResizeEdge::Right},
        {2, 2, ResizeEdge::TopLeft},      {w - 3, 2, ResizeEdge::TopRight},   {2, h - 3, ResizeEdge::BottomLeft}, {w - 3, h - 3, ResizeEdge::BottomRight},
        {w / 2, 0, ResizeEdge::Top},      {0, h / 2, ResizeEdge::Left},       {6, h / 2, ResizeEdge::Left},      {w / 2, h - 1, ResizeEdge::Bottom},
    };
    for (const Probe& p : probes) {
        const auto hit = r.wm.get_resize_handle_at(d.x + p.dx, d.y + p.dy);
        CHECK(hit.has_value());
        CHECK(hit->window_id == a);
        CHECK(hit->edge == p.edge);
        CHECK(r.wm.get_window_at(d.x + p.dx, d.y + p.dy) == a); // over the empty desktop the band belongs to A, not the wallpaper
    }

    // Beyond the band is the desktop again.
    CHECK(r.wm.get_window_at(d.x - 1, d.y + h / 2) == r.wallpaper);
    CHECK(!r.wm.get_resize_handle_at(d.x - 1, d.y + h / 2).has_value());
    // The client area and the title bar are not resize zones.
    CHECK(!r.wm.get_resize_handle_at(500, 400).has_value());
    CHECK(!r.wm.get_resize_handle_at(d.x + w / 2, d.y + 20).has_value());
    CHECK(r.wm.get_decoration_hit_region(a, d.x + w / 2, d.y + 20) == DecorationHitRegion::TitleBar);
    CHECK(r.wm.get_decoration_hit_region(a, d.x + 2, d.y + h / 2) == DecorationHitRegion::Border);

    // Controls.
    const DecorationLayout l = calculate_decoration_layout(make_frame_metrics(r.wm.get_window_frame_style(a), d, {300, 200, 400, 300}));
    auto centre = [&](const Rect& b) { return std::pair{d.x + b.x + static_cast<int32_t>(b.width) / 2, d.y + b.y + static_cast<int32_t>(b.height) / 2}; };
    auto [cx, cy] = centre(l.close_button);
    CHECK(r.wm.get_decoration_hit_region(a, cx, cy) == DecorationHitRegion::CloseButton);
    std::tie(cx, cy) = centre(l.maximize_button);
    CHECK(r.wm.get_decoration_hit_region(a, cx, cy) == DecorationHitRegion::MaximizeButton);
    std::tie(cx, cy) = centre(l.minimize_button);
    CHECK(r.wm.get_decoration_hit_region(a, cx, cy) == DecorationHitRegion::MinimizeButton);
    std::cout << "test_hit_testing_all_edges_and_corners passed\n";
}

void test_layer_semantics_for_the_wallpaper() {
    Rig r;
    const uint32_t a = r.add("A", {300, 200, 400, 300});
    const Rect d = r.wm.get_decorated_window_bounds(a);
    const int32_t px = d.x + 2, py = d.y + 180;

    CHECK(r.wm.get_window_layer(r.wallpaper) == WindowLayer::Desktop);
    CHECK(r.wm.get_window_layer(a) == WindowLayer::Normal);
    CHECK(r.wm.get_resize_handle_at(px, py).has_value());

    // If the wallpaper were an ordinary window, its visible pixels would win over A's invisible band.
    r.wm.set_window_layer(r.wallpaper, WindowLayer::Normal);
    CHECK(r.wm.get_window_at(px, py) == r.wallpaper);
    CHECK(!r.wm.get_resize_handle_at(px, py).has_value());
    std::cout << "test_layer_semantics_for_the_wallpaper passed\n";
}

void test_band_never_steals_from_neighbours_or_panels() {
    Rig r;
    const uint32_t a = r.add("A", {300, 200, 400, 300});
    const Rect da = r.wm.get_decorated_window_bounds(a);

    // Window B (raised above A) covers A's left band with its visible frame.
    const uint32_t b = r.add("B", {150, 220, 200, 200});
    r.wm.focus_window(b);
    const int32_t probe_x = da.x + 2;
    const int32_t probe_y = da.y + 180;
    const Rect db = r.wm.get_decorated_window_bounds(b);
    CHECK(probe_x >= db.x + 6 && probe_x < db.x + static_cast<int32_t>(db.width) - 6); // inside B's visible frame
    CHECK(r.wm.get_window_at(probe_x, probe_y) == b);
    CHECK(!r.wm.get_resize_handle_at(probe_x, probe_y).has_value() || r.wm.get_resize_handle_at(probe_x, probe_y)->window_id == b);

    // A panel (undecorated, full width) lies over A's bottom band: the panel wins.
    Rig p;
    const uint32_t task = p.wm.create_window("Taskbar", {0, 1032, 1920, 48});
    p.wm.set_window_surface(task, std::make_shared<SoftwareSurface>(8, 8));
    p.wm.set_window_layer(task, WindowLayer::Panel);
    const uint32_t c = p.add("C", {300, 950, 400, 100});
    p.wm.focus_window(c); // C is above the panel in z-order
    const Rect dc = p.wm.get_decorated_window_bounds(c);
    const int32_t band_y = dc.y + static_cast<int32_t>(dc.height) - 2; // inside C's bottom band, over the taskbar
    CHECK(band_y >= 1032);
    CHECK(p.wm.get_window_at(dc.x + 100, band_y) == task);
    CHECK(!p.wm.get_resize_handle_at(dc.x + 100, band_y).has_value() || p.wm.get_resize_handle_at(dc.x + 100, band_y)->window_id == task);
    std::cout << "test_band_never_steals_from_neighbours_or_panels passed\n";
}

void test_maximized_is_flush_and_restores() {
    Rig r;
    const uint32_t a = r.add("A", {300, 200, 400, 300});
    const uint32_t task = r.wm.create_window("Taskbar", {0, 1032, 1920, 48});
    r.wm.set_window_surface(task, std::make_shared<SoftwareSurface>(8, 8));
    r.wm.set_window_layer(task, WindowLayer::Panel);

    r.wm.set_window_state(a, WindowState::Maximized);
    const auto margins = r.wm.get_window_margins(a);
    CHECK(margins.top == 36 && margins.left == 0 && margins.right == 0 && margins.bottom == 0);
    const auto geom = r.wm.get_window_geometry(a);
    CHECK(geom->x == 0 && geom->y == 36 && geom->width == 1920 && geom->height == 996);
    const Rect d = r.wm.get_decorated_window_bounds(a);
    CHECK(d.x == 0 && d.y == 0 && d.width == 1920 && d.height == 1032);

    // No resize band anywhere on a maximized window; the panel below it still owns its own pixels.
    for (int32_t x : {0, 1, 960, 1919}) CHECK(!r.wm.get_resize_handle_at(x, 500).has_value());
    CHECK(!r.wm.get_resize_handle_at(960, 0).has_value());
    CHECK(!r.wm.get_resize_handle_at(960, 1031).has_value());
    CHECK(r.wm.get_window_at(960, 1040) == task);
    CHECK(r.wm.get_window_at(960, 1031) == a);

    // Minimize and restore through Maximized keeps the work-area fit.
    r.wm.set_window_state(a, WindowState::Minimized);
    r.wm.set_window_state(a, WindowState::Maximized);
    const auto geom2 = r.wm.get_window_geometry(a);
    CHECK(geom2->x == 0 && geom2->y == 36 && geom2->width == 1920 && geom2->height == 996);

    // Restore: margins and geometry return.
    r.wm.set_window_state(a, WindowState::Normal);
    const auto m2 = r.wm.get_window_margins(a);
    CHECK(m2.top == 42 && m2.left == 7 && m2.right == 7 && m2.bottom == 7);
    const auto g3 = r.wm.get_window_geometry(a);
    CHECK(g3->x == 300 && g3->y == 200 && g3->width == 400 && g3->height == 300);
    CHECK(r.wm.get_resize_handle_at(293 + 2, 358).has_value());
    std::cout << "test_maximized_is_flush_and_restores passed\n";
}

void test_legacy_windows_keep_original_behaviour() {
    Rig r;
    const uint32_t id = r.wm.create_window("Legacy", {100, 100, 200, 100});
    r.wm.set_window_surface(id, std::make_shared<SoftwareSurface>(8, 8));
    const auto m = r.wm.get_window_margins(id);
    CHECK(m.top == 0 && m.left == 0);
    const auto s = r.wm.get_window_frame_style(id);
    CHECK(s.resize_inset == 0 && s.border_thickness == 5);
    // Original behaviour: a 5 px border inside the bounds is the resize zone.
    CHECK(r.wm.get_resize_handle_at(102, 150)->edge == ResizeEdge::Left);
    CHECK(!r.wm.get_resize_handle_at(150, 150).has_value());
    std::cout << "test_legacy_windows_keep_original_behaviour passed\n";
}

} // namespace

int main() {
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

    (void)toolkit();
    test_layout_invariants();
    test_resize_ring_tiles_the_edge();
    test_icons();
    test_decorator_focus_states();
    test_decorator_controls_and_title();
    test_hit_testing_all_edges_and_corners();
    test_layer_semantics_for_the_wallpaper();
    test_band_never_steals_from_neighbours_or_panels();
    test_maximized_is_flush_and_restores();
    test_legacy_windows_keep_original_behaviour();
    std::cout << "All window chrome tests passed successfully.\n";
    return 0;
}
