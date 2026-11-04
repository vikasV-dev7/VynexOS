// Layered stacking in the window manager: one stacking order, sorted by layer (Desktop < Normal < Panel < Overlay),
// raised within a layer by focus. Composition (build_scene + the compositor) and hit-testing both follow it, so what
// is on top on screen is what receives the pointer, invisible resize bands included.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/core/logger.hpp"

#include "test_check.hpp"

using namespace vynexos;
using namespace vynexos::desktop;

namespace {

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

class CapturingDisplay final : public IDisplayAbstraction {
public:
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last = buffer;
        return {};
    }
    void shutdown() override {}
    hal::FrameBuffer last{0, 0, {}};
};

std::shared_ptr<SoftwareSurface> solid(uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b) {
    auto s = std::make_shared<SoftwareSurface>(w, h);
    auto px = s->map_pixels();
    for (size_t i = 0; i < px.size(); i += 4) px[i] = r, px[i + 1] = g, px[i + 2] = b, px[i + 3] = 255;
    s->unmap_pixels();
    return s;
}
FrameStyle style_for(WindowState s) {
    BasicWindowDecorator decorator(std::make_shared<BasicWidgetToolkit>());
    return decorator.frame_style(s);
}

struct Rig {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    BasicWindowManager wm{bus, logger};

    uint32_t add(const char* title, WindowGeometry g, WindowLayer layer, uint8_t r, uint8_t gr, uint8_t b) {
        const uint32_t id = wm.create_window(title, g);
        wm.set_window_surface(id, solid(g.width, g.height, r, gr, b));
        if (layer != WindowLayer::Normal) wm.set_window_layer(id, layer);
        return id;
    }
    uint32_t add_framed(const char* title, WindowGeometry g) {
        const uint32_t id = wm.create_window(title, g);
        wm.set_window_surface(id, std::make_shared<SoftwareSurface>(8, 8));
        wm.set_window_frame_styles(id, style_for(WindowState::Normal), style_for(WindowState::Maximized));
        return id;
    }
    bool sorted_by_layer() const {
        const auto z = wm.get_windows_z_ordered();
        for (size_t i = 1; i < z.size(); ++i) {
            if (wm.get_window_layer(z[i - 1]) > wm.get_window_layer(z[i])) return false;
        }
        return true;
    }
    /// The window ids of the scene, in composition order, one entry per window.
    std::vector<uint32_t> scene_windows() const {
        std::vector<uint32_t> ids;
        for (const auto& layer : wm.build_scene()) {
            if (ids.empty() || ids.back() != layer.window_id) ids.push_back(layer.window_id);
        }
        return ids;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
void test_stacking_is_sorted_by_layer() {
    Rig r;
    // Created in a scrambled order on purpose.
    const uint32_t overlay = r.add("Overlay", {0, 0, 10, 10}, WindowLayer::Overlay, 1, 1, 1);
    const uint32_t a = r.add("A", {0, 0, 10, 10}, WindowLayer::Normal, 1, 1, 1);
    const uint32_t panel = r.add("Panel", {0, 0, 10, 10}, WindowLayer::Panel, 1, 1, 1);
    const uint32_t desktop = r.add("Desktop", {0, 0, 10, 10}, WindowLayer::Desktop, 1, 1, 1);
    const uint32_t b = r.add("B", {0, 0, 10, 10}, WindowLayer::Normal, 1, 1, 1);
    vynex_test::context() = "after creation";
    CHECK((r.wm.get_windows_z_ordered() == std::vector<uint32_t>{desktop, a, b, panel, overlay}));

    vynex_test::context() = "focus raises within the layer only";
    r.wm.focus_window(a);
    CHECK((r.wm.get_windows_z_ordered() == std::vector<uint32_t>{desktop, b, a, panel, overlay}));
    r.wm.focus_window(desktop); // the desktop stays at the bottom
    r.wm.focus_window(panel);   // a panel stays below overlays
    CHECK((r.wm.get_windows_z_ordered() == std::vector<uint32_t>{desktop, b, a, panel, overlay}));
    const uint32_t c = r.add("C", {0, 0, 10, 10}, WindowLayer::Normal, 1, 1, 1); // a new window: top of Normal
    CHECK((r.wm.get_windows_z_ordered() == std::vector<uint32_t>{desktop, b, a, c, panel, overlay}));

    vynex_test::context() = "changing layer puts the window on top of its new layer";
    r.wm.set_window_layer(a, WindowLayer::Overlay);
    CHECK((r.wm.get_windows_z_ordered() == std::vector<uint32_t>{desktop, b, c, panel, overlay, a}));
    r.wm.set_window_layer(a, WindowLayer::Normal);
    CHECK((r.wm.get_windows_z_ordered() == std::vector<uint32_t>{desktop, b, c, a, panel, overlay}));

    vynex_test::context() = "a storm of focus changes keeps the order sorted by layer";
    std::mt19937 rng(7);
    const std::vector<uint32_t> all{overlay, a, panel, desktop, b, c};
    for (int i = 0; i < 2000; ++i) {
        r.wm.focus_window(all[rng() % all.size()]);
        CHECK(r.sorted_by_layer());
    }
    CHECK(r.wm.get_windows_z_ordered().size() == all.size());
    r.wm.destroy_window(b);
    CHECK(r.sorted_by_layer() && r.wm.get_windows_z_ordered().size() == all.size() - 1);
    CHECK(!r.wm.get_active_window().has_value() || *r.wm.get_active_window() != b); // never a destroyed window
    std::printf("test_stacking_is_sorted_by_layer passed\n");
}

void test_scene_follows_the_stacking_order() {
    Rig r;
    const uint32_t desktop = r.add("Desktop", {0, 0, 1920, 1080}, WindowLayer::Desktop, 30, 30, 30);
    const uint32_t panel = r.add("Panel", {0, 1040, 1920, 40}, WindowLayer::Panel, 0, 200, 0);
    const uint32_t overlay = r.add("Overlay", {10, 640, 300, 400}, WindowLayer::Overlay, 200, 200, 0);
    const uint32_t a = r.add("A", {200, 200, 400, 300}, WindowLayer::Normal, 200, 0, 0);
    const uint32_t b = r.add("B", {300, 300, 400, 300}, WindowLayer::Normal, 0, 0, 200);
    vynex_test::context() = "scene order";
    CHECK((r.scene_windows() == std::vector<uint32_t>{desktop, a, b, panel, overlay}));
    r.wm.focus_window(a);
    CHECK((r.scene_windows() == std::vector<uint32_t>{desktop, b, a, panel, overlay}));

    vynex_test::context() = "hidden and minimized windows take no part";
    r.wm.set_window_visibility(overlay, false);
    r.wm.set_window_state(b, WindowState::Minimized);
    CHECK((r.scene_windows() == std::vector<uint32_t>{desktop, a, panel}));
    r.wm.set_window_visibility(overlay, true);
    r.wm.set_window_state(b, WindowState::Normal);
    CHECK((r.scene_windows() == std::vector<uint32_t>{desktop, b, a, panel, overlay})); // shown again on top of its layer
    std::printf("test_scene_follows_the_stacking_order passed\n");
}

void test_composition_and_hit_testing_agree_on_screen() {
    Rig r;
    auto display = std::make_shared<CapturingDisplay>();
    BasicCompositor compositor(display, r.logger);
    // Undecorated windows (no bands): each window covers exactly its pixels.
    const uint32_t desktop = r.add("Desktop", {0, 0, 1920, 1080}, WindowLayer::Desktop, 10, 10, 200);
    const uint32_t a = r.add("A", {200, 900, 400, 300}, WindowLayer::Normal, 200, 0, 0);    // reaches under the panel
    const uint32_t b = r.add("B", {450, 800, 400, 300}, WindowLayer::Normal, 0, 0, 90);     // overlaps A
    const uint32_t panel = r.add("Panel", {0, 1000, 1920, 80}, WindowLayer::Panel, 0, 200, 0);
    const uint32_t overlay = r.add("Overlay", {100, 950, 300, 100}, WindowLayer::Overlay, 200, 200, 0);
    struct Colour {
        uint32_t window;
        uint8_t r, g, b;
    };
    const Colour colours[] = {{desktop, 10, 10, 200}, {a, 200, 0, 0}, {b, 0, 0, 90}, {panel, 0, 200, 0}, {overlay, 200, 200, 0}};
    auto shown_at = [&](int32_t x, int32_t y) -> uint32_t {
        const size_t i = (static_cast<size_t>(y) * display->last.width + static_cast<size_t>(x)) * 4;
        for (const auto& c : colours) {
            if (display->last.pixels[i] == c.r && display->last.pixels[i + 1] == c.g && display->last.pixels[i + 2] == c.b) return c.window;
        }
        return 0;
    };
    auto render = [&] {
        compositor.invalidate_region({0, 0, 1920, 1080});
        CHECK(compositor.render_frame(r.wm.build_scene()).has_value());
    };
    auto expect_agreement = [&](const char* what) {
        // On a grid over the whole screen, the window the pointer reaches is the window whose pixels are shown there.
        int checked = 0;
        for (int32_t y = 5; y < 1080; y += 23) {
            for (int32_t x = 5; x < 1920; x += 37) {
                const uint32_t shown = shown_at(x, y);
                const auto hit = r.wm.get_window_at(x, y);
                if (!(shown != 0 && hit == std::optional<uint32_t>{shown})) {
                    vynex_test::context() = std::string(what) + " at (" + std::to_string(x) + "," + std::to_string(y) + ")";
                    CHECK(false);
                }
                ++checked;
            }
        }
        CHECK(checked > 2000);
    };

    render();
    vynex_test::context() = "panel over a normal window, overlay over both";
    CHECK(shown_at(420, 1020) == panel);   // A under the panel (clear of the overlay and B): the panel shows
    CHECK(shown_at(150, 1010) == overlay); // overlay over the panel and A
    CHECK(shown_at(300, 920) == a);
    CHECK(shown_at(500, 950) == b);        // B was created later: above A
    CHECK(shown_at(1500, 300) == desktop);
    expect_agreement("initial");

    r.wm.focus_window(a); // A rises above B, but not above the panel or the overlay
    render();
    vynex_test::context() = "after focusing A";
    CHECK(shown_at(500, 950) == a);
    CHECK(shown_at(420, 1020) == panel && shown_at(150, 1010) == overlay);
    expect_agreement("after focusing A");

    r.wm.set_window_visibility(overlay, false); // a hidden overlay neither shows nor receives
    render();
    CHECK(shown_at(150, 1010) == panel);
    expect_agreement("overlay hidden");

    r.wm.set_window_state(a, WindowState::Minimized);
    render();
    CHECK(shown_at(300, 920) == desktop && shown_at(500, 950) == b);
    expect_agreement("A minimized");
    std::printf("test_composition_and_hit_testing_agree_on_screen passed\n");
}

void test_resize_band_priority() {
    Rig r;
    r.wm.set_work_area({0, 0, 1920, 1040});
    const uint32_t wallpaper = r.add("Wallpaper", {0, 0, 1920, 1080}, WindowLayer::Desktop, 30, 30, 30);
    const uint32_t lower = r.add_framed("Lower", {100, 200, 400, 300});
    const uint32_t upper = r.add_framed("Upper", {500, 220, 400, 300}); // its left band lies over Lower's visible frame
    const Rect du = r.wm.get_decorated_window_bounds(upper);
    const Rect dl = r.wm.get_decorated_window_bounds(lower);
    const int32_t px = du.x + 2, py = du.y + 150; // inside Upper's invisible left band...
    CHECK(px >= dl.x + 6 && px < dl.x + static_cast<int32_t>(dl.width) - 6); // ...and inside Lower's visible frame

    vynex_test::context() = "the upper window's band does not yield to the visible window beneath";
    CHECK(r.wm.get_window_at(px, py) == std::optional<uint32_t>{upper});
    auto handle = r.wm.get_resize_handle_at(px, py);
    CHECK(handle.has_value() && handle->window_id == upper && handle->edge == ResizeEdge::Left);

    vynex_test::context() = "once the lower window is raised, its visible frame wins";
    r.wm.focus_window(lower);
    CHECK(r.wm.get_window_at(px, py) == std::optional<uint32_t>{lower});
    handle = r.wm.get_resize_handle_at(px, py);
    CHECK(!handle.has_value() || handle->window_id == lower);

    vynex_test::context() = "over the empty desktop the band belongs to its window";
    const int32_t qx = du.x + static_cast<int32_t>(du.width) - 2, qy = du.y + 150; // Upper's right band
    CHECK(r.wm.get_window_at(qx, qy) == std::optional<uint32_t>{upper});
    CHECK(r.wm.get_resize_handle_at(qx, qy).has_value() && r.wm.get_resize_handle_at(qx, qy)->edge == ResizeEdge::Right);

    vynex_test::context() = "a panel and an overlay keep their pixels from any window's band";
    const uint32_t panel = r.add("Panel", {0, 1040, 1920, 40}, WindowLayer::Panel, 0, 200, 0);
    const uint32_t low = r.add_framed("Low", {300, 940, 400, 97}); // bottom band reaches below y = 1040
    r.wm.focus_window(low);
    const Rect dlow = r.wm.get_decorated_window_bounds(low);
    const int32_t band_y = dlow.y + static_cast<int32_t>(dlow.height) - 2;
    CHECK(band_y >= 1040);
    CHECK(r.wm.get_window_at(dlow.x + 100, band_y) == std::optional<uint32_t>{panel});
    // (Undecorated windows keep the default frame style's own edge ring, so the panel may report its own edge; what
    // matters is that the point is not Low's resize band.)
    const auto under_panel = r.wm.get_resize_handle_at(dlow.x + 100, band_y);
    CHECK(!under_panel.has_value() || under_panel->window_id == panel);
    const uint32_t overlay = r.add("Overlay", {du.x - 50, du.y + 100, 60, 100}, WindowLayer::Overlay, 200, 200, 0); // covers Upper's left band
    r.wm.focus_window(upper);
    CHECK(r.wm.get_window_at(du.x + 2, du.y + 150) == std::optional<uint32_t>{overlay});
    const auto under_overlay = r.wm.get_resize_handle_at(du.x + 2, du.y + 150);
    CHECK(!under_overlay.has_value() || under_overlay->window_id == overlay);

    vynex_test::context() = "a maximized window has no band; the panel below it owns its pixels";
    r.wm.set_window_visibility(overlay, false);
    r.wm.set_window_state(upper, WindowState::Maximized);
    const auto max = *r.wm.get_window_geometry(upper);
    CHECK(r.wm.get_window_at(max.x + 5, static_cast<int32_t>(max.y + max.height) + 3) == std::optional<uint32_t>{panel});
    CHECK(!r.wm.get_resize_handle_at(max.x + 5, max.y + 5).has_value());
    (void)wallpaper;
    std::printf("test_resize_band_priority passed\n");
}

} // namespace

int main() {
    std::printf("Running layer and hit-testing tests...\n");
    test_stacking_is_sorted_by_layer();
    test_scene_follows_the_stacking_order();
    test_composition_and_hit_testing_agree_on_screen();
    test_resize_band_priority();
    std::printf("All layer and hit-testing tests passed.\n");
    return 0;
}
