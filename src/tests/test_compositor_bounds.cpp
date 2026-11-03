// Compositor memory safety and clipping correctness at the edges of the framebuffer: windows partly or wholly off-screen
// on every side and corner, negative coordinates, explicit clips that reach off-screen, zero-sized and inconsistent
// rectangles and surfaces. The compositor's output is compared, over the whole framebuffer, with an independent reference
// that computes every pixel with 64-bit bounds checks. Source surfaces carry guard bytes; Debug builds also check the CRT
// heap. Checks use CHECK (test_check.hpp), which is active in every build configuration, including Release with NDEBUG.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "../apps/desktop_shell.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_taskbar.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/desktop/decoration_layout.hpp"
#include "vynexos/hal/input_driver.hpp"

#include "test_check.hpp"

using namespace vynexos;
using namespace vynexos::desktop;

namespace {

// What the current check is about, printed by a failing CHECK.
std::string& g_context = vynex_test::context();

void check_heap() {
#if defined(_MSC_VER) && defined(_DEBUG)
    CHECK(_CrtCheckMemory()); // validates the guard bytes around every heap block
#endif
}

class TestLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

/// A display of any logical size that keeps the last presented frame.
class CapturingDisplay final : public IDisplayAbstraction {
public:
    explicit CapturingDisplay(hal::DisplaySize size = hal::kDefaultDisplaySize) : m_size(size) {}
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last = buffer;
        ++flushes;
        return {};
    }
    void shutdown() override {}
    hal::DisplaySize logical_size() const override { return m_size; }
    hal::FrameBuffer last{0, 0, {}};
    int flushes{0};

private:
    hal::DisplaySize m_size;
};

/// A surface whose pixels sit between two guard regions, so writes outside the pixels are detected.
class GuardedSurface final : public ISurface {
public:
    static constexpr size_t kGuard = 256;
    static constexpr uint8_t kPattern = 0xA5;
    GuardedSurface(uint32_t w, uint32_t h) : m_w(w), m_h(h), m_buf(kGuard + static_cast<size_t>(w) * h * 4 + kGuard, kPattern) {
        std::fill(m_buf.begin() + kGuard, m_buf.end() - kGuard, uint8_t{0});
    }
    uint32_t width() const override { return m_w; }
    uint32_t height() const override { return m_h; }
    std::span<uint8_t> map_pixels() override { return {m_buf.data() + kGuard, static_cast<size_t>(m_w) * m_h * 4}; }
    void unmap_pixels() override {}
    bool guards_intact() const {
        return std::all_of(m_buf.begin(), m_buf.begin() + kGuard, [](uint8_t b) { return b == kPattern; }) &&
               std::all_of(m_buf.end() - kGuard, m_buf.end(), [](uint8_t b) { return b == kPattern; });
    }

private:
    uint32_t m_w, m_h;
    std::vector<uint8_t> m_buf;
};

/// Declares a size its pixel memory does not have: the compositor must not read past what map_pixels() returns.
class LyingSurface final : public ISurface {
public:
    uint32_t width() const override { return 50; }
    uint32_t height() const override { return 50; }
    std::span<uint8_t> map_pixels() override { return {m_px.data(), m_px.size()}; }
    void unmap_pixels() override {}

private:
    std::vector<uint8_t> m_px = std::vector<uint8_t>(10 * 10 * 4, 200);
};

/// Fills a surface with a deterministic pattern. `band` transparent pixels around the edge, partial alpha just inside
/// (like the anti-aliased edge of rounded chrome), opaque elsewhere, unless `all_alpha` is given.
void paint(ISurface& s, uint32_t seed, uint32_t band = 0, int all_alpha = -1) {
    auto px = s.map_pixels();
    const uint32_t w = s.width(), h = s.height();
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4;
            px[i] = static_cast<uint8_t>(seed * 37 + x * 7);
            px[i + 1] = static_cast<uint8_t>(seed * 11 + y * 13);
            px[i + 2] = static_cast<uint8_t>(seed * 5 + (x ^ y) * 3);
            const uint32_t edge = std::min({x, y, w - 1 - x, h - 1 - y});
            uint8_t a = 255;
            if (edge < band) a = 0;
            else if (edge == band) a = static_cast<uint8_t>(90 + (x * 17 + y * 29) % 120);
            if (all_alpha >= 0) a = static_cast<uint8_t>(all_alpha);
            px[i + 3] = a;
        }
    }
    s.unmap_pixels();
}

// ---------------------------------------------------------------------------------------------------------------------
// Reference: the composited value of every pixel, computed independently with 64-bit bounds checks per pixel.
// ---------------------------------------------------------------------------------------------------------------------
bool inside(const Rect& r, int64_t x, int64_t y) {
    return x >= r.x && y >= r.y && x < static_cast<int64_t>(r.x) + r.width && y < static_cast<int64_t>(r.y) + r.height;
}

std::vector<uint8_t> reference_frame(const SceneGraph& scene, uint32_t w, uint32_t h) {
    std::vector<uint8_t> fb(static_cast<size_t>(w) * h * 4, 0);
    for (const auto& layer : scene) {
        if (!layer.visible || layer.opacity <= 0.0f || !layer.surface) continue;
        auto px = layer.surface->map_pixels();
        const uint32_t sw = layer.surface->width(), sh = layer.surface->height();
        if (px.size() < static_cast<size_t>(sw) * sh * 4 || px.empty()) continue;
        const bool unclipped = layer.clip.width == 0 || layer.clip.height == 0;
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                if (!inside(layer.bounds, x, y)) continue;
                if (!unclipped && !inside(layer.clip, x, y)) continue;
                const int64_t sx = static_cast<int64_t>(x) - layer.bounds.x, sy = static_cast<int64_t>(y) - layer.bounds.y;
                if (sx < 0 || sy < 0 || sx >= sw || sy >= sh) continue;
                const size_t si = (static_cast<size_t>(sy) * sw + static_cast<size_t>(sx)) * 4;
                const size_t di = (static_cast<size_t>(y) * w + x) * 4;
                const uint8_t sa = layer.opacity == 1.0f ? px[si + 3] : static_cast<uint8_t>(px[si + 3] * layer.opacity);
                if (sa == 255) {
                    fb[di] = px[si];
                    fb[di + 1] = px[si + 1];
                    fb[di + 2] = px[si + 2];
                    fb[di + 3] = 255;
                } else if (sa > 0) {
                    const float alpha = sa / 255.0f;
                    const float inv_alpha = 1.0f - alpha;
                    fb[di] = static_cast<uint8_t>((px[si] * alpha) + (fb[di] * inv_alpha));
                    fb[di + 1] = static_cast<uint8_t>((px[si + 1] * alpha) + (fb[di + 1] * inv_alpha));
                    fb[di + 2] = static_cast<uint8_t>((px[si + 2] * alpha) + (fb[di + 2] * inv_alpha));
                    fb[di + 3] = std::max(fb[di + 3], sa);
                }
            }
        }
    }
    return fb;
}

/// Index of the first differing pixel, or -1.
int64_t first_difference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return 0;
    for (size_t i = 0; i < a.size(); i += 4) {
        if (std::memcmp(&a[i], &b[i], 4) != 0) return static_cast<int64_t>(i / 4);
    }
    return -1;
}

/// A small framebuffer keeps exhaustive sweeps cheap; every edge is still an edge.
struct SmallRig {
    static constexpr uint32_t kW = 64, kH = 40;
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    std::shared_ptr<CapturingDisplay> display = std::make_shared<CapturingDisplay>(hal::DisplaySize{kW, kH});
    BasicCompositor compositor{display, logger};

    void expect_matches_reference(const SceneGraph& scene) {
        const auto expected = reference_frame(scene, kW, kH);
        CHECK(display->last.width == kW && display->last.height == kH);
        const int64_t diff = first_difference(display->last.pixels, expected);
        if (diff >= 0) {
            g_context += " | first differing pixel x=" + std::to_string(diff % kW) + " y=" + std::to_string(diff / kW);
        }
        CHECK(diff < 0);
    }
};

/// A window as the window manager emits it: a decoration layer (transparent band, explicit clip = visible frame, which
/// may reach off-screen) and a content layer inside it; plus a half-transparent overlay to exercise the opacity path.
struct Window {
    static constexpr uint32_t kDecoW = 30, kDecoH = 22, kBand = 3, kTitle = 6;
    std::shared_ptr<GuardedSurface> deco = std::make_shared<GuardedSurface>(kDecoW, kDecoH);
    std::shared_ptr<GuardedSurface> content = std::make_shared<GuardedSurface>(kDecoW - 2 * (kBand + 1), kDecoH - kTitle - kBand - 1 - kBand);
    std::shared_ptr<GuardedSurface> overlay = std::make_shared<GuardedSurface>(12, 9);
    Window() {
        paint(*deco, 1, kBand);
        paint(*content, 2);
        paint(*overlay, 3, 0, 200);
    }
    Rect bounds(int32_t x, int32_t y) const { return {x, y, kDecoW, kDecoH}; }
    SceneGraph scene_at(const std::shared_ptr<ISurface>& wallpaper, int32_t x, int32_t y) const {
        SceneGraph s(4);
        s[0].bounds = {0, 0, SmallRig::kW, SmallRig::kH};
        s[0].clip = {0, 0, 0, 0};
        s[0].surface = wallpaper;
        s[1].bounds = bounds(x, y);
        s[1].clip = {x + static_cast<int32_t>(kBand), y + static_cast<int32_t>(kBand), kDecoW - 2 * kBand, kDecoH - 2 * kBand};
        s[1].surface = deco;
        s[2].bounds = {x + static_cast<int32_t>(kBand + 1), y + static_cast<int32_t>(kTitle + kBand), content->width(), content->height()};
        s[2].clip = {0, 0, 0, 0};
        s[2].surface = content;
        s[3].bounds = {x + 20, y - 4, overlay->width(), overlay->height()};
        s[3].clip = {0, 0, 0, 0};
        s[3].opacity = 0.5f;
        s[3].surface = overlay;
        return s;
    }
    Rect footprint(int32_t x, int32_t y) const { return {x, y - 4, 34, kDecoH + 4}; } // decoration + overlay
    bool guards_intact() const { return deco->guards_intact() && content->guards_intact() && overlay->guards_intact(); }
};

std::shared_ptr<GuardedSurface> make_wallpaper() {
    auto w = std::make_shared<GuardedSurface>(SmallRig::kW, SmallRig::kH);
    paint(*w, 9);
    return w;
}

// ---------------------------------------------------------------------------------------------------------------------
// Edges, corners, partial and full off-screen placement
// ---------------------------------------------------------------------------------------------------------------------
void test_edges_and_corners() {
    SmallRig rig;
    Window win;
    auto wallpaper = make_wallpaper();
    const int32_t W = SmallRig::kW, H = SmallRig::kH, w = Window::kDecoW, h = Window::kDecoH;
    struct Case {
        const char* name;
        int32_t x, y;
    };
    const Case cases[] = {
        {"on screen", 17, 9},
        {"left edge, partly off", -12, 9},           {"right edge, partly off", W - 10, 9},
        {"top edge, partly off", 17, -11},            {"bottom edge, partly off", 17, H - 8},
        {"top-left corner", -12, -11},                {"top-right corner", W - 10, -11},
        {"bottom-left corner", -12, H - 8},           {"bottom-right corner", W - 10, H - 8},
        {"left, fully off", -w - 5, 9},               {"right, fully off", W + 5, 9},
        {"top, fully off", 17, -h - 9},               {"bottom, fully off", 17, H + 6},
        {"touching left edge from outside", -w, 9},   {"touching top edge from outside", 17, -h},
        {"far negative", -100000, -100000},           {"far positive", 100000, 100000},
        // Far enough from the limits that the test's own window arithmetic (x + 20 etc.) cannot overflow; the compositor
        // still sees bounds whose right/bottom edge is near the end of int32 (and invalid-input cases go to the limit).
        {"near INT32_MIN", std::numeric_limits<int32_t>::min() + 100, std::numeric_limits<int32_t>::min() + 100},
        {"near INT32_MAX", std::numeric_limits<int32_t>::max() - 100, std::numeric_limits<int32_t>::max() - 100},
        {"back on screen", 17, 9},
    };
    int32_t px = 17, py = 9;
    for (const Case& c : cases) {
        g_context = std::string("edge case '") + c.name + "'";
        // What the shell does on a move: invalidate the old and the new footprint (either may be off-screen).
        rig.compositor.invalidate_region(win.footprint(px, py));
        rig.compositor.invalidate_region(win.footprint(c.x, c.y));
        const auto scene = win.scene_at(wallpaper, c.x, c.y);
        CHECK(rig.compositor.render_frame(scene).has_value());
        rig.expect_matches_reference(scene);
        px = c.x;
        py = c.y;
    }
    g_context = "guards after edge cases";
    CHECK(win.guards_intact() && wallpaper->guards_intact());
    check_heap();
    std::printf("test_edges_and_corners passed (%zu placements)\n", std::size(cases));
}

/// Every placement from fully off the left/top to fully off the right/bottom, moving like a drag, compared every frame.
void test_exhaustive_drag_sweep() {
    SmallRig rig;
    Window win;
    auto wallpaper = make_wallpaper();
    const int32_t x0 = -static_cast<int32_t>(Window::kDecoW) - 6, x1 = static_cast<int32_t>(SmallRig::kW) + 6;
    const int32_t y0 = -static_cast<int32_t>(Window::kDecoH) - 6, y1 = static_cast<int32_t>(SmallRig::kH) + 8;
    int32_t px = x0, py = y0;
    int frames = 0;
    for (int32_t y = y0; y <= y1; y += 2) {
        // Alternate direction so consecutive frames are neighbouring positions, as in a real drag.
        const bool forward = ((y - y0) / 2) % 2 == 0;
        for (int32_t i = 0; i <= x1 - x0; ++i) {
            const int32_t x = forward ? x0 + i : x1 - i;
            rig.compositor.invalidate_region(win.footprint(px, py));
            rig.compositor.invalidate_region(win.footprint(x, y));
            const auto scene = win.scene_at(wallpaper, x, y);
            g_context = "sweep at x=" + std::to_string(x) + " y=" + std::to_string(y);
            CHECK(rig.compositor.render_frame(scene).has_value());
            rig.expect_matches_reference(scene);
            px = x;
            py = y;
            ++frames;
        }
    }
    g_context = "guards after sweep";
    CHECK(win.guards_intact() && wallpaper->guards_intact());
    check_heap();
    std::printf("test_exhaustive_drag_sweep passed (%d frames, each compared over the whole framebuffer)\n", frames);
}

// ---------------------------------------------------------------------------------------------------------------------
// Zero-sized, invalid and inconsistent inputs
// ---------------------------------------------------------------------------------------------------------------------
void test_zero_sized_and_invalid_inputs() {
    SmallRig rig;
    auto wallpaper = make_wallpaper();
    auto small = std::make_shared<GuardedSurface>(10, 10);
    paint(*small, 4);
    auto empty = std::make_shared<SoftwareSurface>(0, 0);
    auto lying = std::make_shared<LyingSurface>();
    constexpr int32_t kMin = std::numeric_limits<int32_t>::min(), kMax = std::numeric_limits<int32_t>::max();
    constexpr uint32_t kHuge = std::numeric_limits<uint32_t>::max();

    struct Case {
        const char* name;
        RenderLayer layer;
    };
    auto layer = [](Rect bounds, Rect clip, std::shared_ptr<ISurface> s, float opacity = 1.0f) {
        RenderLayer l;
        l.bounds = bounds;
        l.clip = clip;
        l.surface = std::move(s);
        l.opacity = opacity;
        return l;
    };
    const Case cases[] = {
        {"zero-sized surface", layer({5, 5, 10, 10}, {0, 0, 0, 0}, empty)},
        {"zero-width bounds", layer({5, 5, 0, 10}, {0, 0, 0, 0}, small)},
        {"zero-height bounds", layer({5, 5, 10, 0}, {0, 0, 0, 0}, small)},
        {"null surface", layer({5, 5, 10, 10}, {0, 0, 0, 0}, nullptr)},
        {"bounds larger than the surface", layer({20, 10, 40, 30}, {0, 0, 0, 0}, small)},
        {"bounds smaller than the surface", layer({20, 10, 4, 3}, {0, 0, 0, 0}, small)},
        {"surface declares more pixels than it has", layer({2, 2, 50, 50}, {0, 0, 0, 0}, lying)},
        {"explicit clip entirely off-screen", layer({5, 5, 10, 10}, {-50, -50, 20, 20}, small)},
        {"explicit clip partly off-screen (negative)", layer({-4, -4, 10, 10}, {-8, -8, 12, 12}, small)},
        {"explicit clip beyond the right/bottom edge", layer({58, 35, 10, 10}, {60, 37, 1000, 1000}, small)},
        {"clip with huge width at INT32_MIN", layer({-3, -3, 10, 10}, {kMin, kMin, kHuge, kHuge}, small)},
        {"bounds near INT32_MAX (x + width overflows int32)", layer({kMax - 5, kMax - 5, 1000, 1000}, {0, 0, 0, 0}, small)},
        {"bounds with huge width at INT32_MIN", layer({kMin, kMin, kHuge, kHuge}, {0, 0, 0, 0}, small)},
        {"negative placement, translucent", layer({-7, -6, 10, 10}, {0, 0, 0, 0}, small, 0.4f)},
        {"invisible layer", [&] { auto l = layer({5, 5, 10, 10}, {0, 0, 0, 0}, small); l.visible = false; return l; }()},
        {"zero opacity", layer({5, 5, 10, 10}, {0, 0, 0, 0}, small, 0.0f)},
    };
    for (const Case& c : cases) {
        g_context = std::string("invalid input '") + c.name + "'";
        SceneGraph scene(2);
        scene[0].bounds = {0, 0, SmallRig::kW, SmallRig::kH};
        scene[0].surface = wallpaper;
        scene[1] = c.layer;
        rig.compositor.invalidate_region({0, 0, SmallRig::kW, SmallRig::kH});
        CHECK(rig.compositor.render_frame(scene).has_value());
        rig.expect_matches_reference(scene);
    }

    // Invalidations that are empty, wholly off-screen or extreme must not draw anything or misbehave.
    SceneGraph scene(1);
    scene[0].bounds = {0, 0, SmallRig::kW, SmallRig::kH};
    scene[0].surface = wallpaper;
    rig.compositor.invalidate_region({0, 0, SmallRig::kW, SmallRig::kH});
    CHECK(rig.compositor.render_frame(scene).has_value());
    const int flushes = rig.display->flushes;
    for (const Rect r : {Rect{3, 3, 0, 5}, Rect{3, 3, 5, 0}, Rect{-100, -100, 50, 50}, Rect{SmallRig::kW, 0, 10, 10},
                         Rect{0, static_cast<int32_t>(SmallRig::kH), 10, 10}, Rect{kMax - 1, kMax - 1, kHuge, kHuge}, Rect{kMin, kMin, 10, 10}}) {
        rig.compositor.invalidate_region(r);
    }
    g_context = "off-screen / empty invalidations";
    CHECK(rig.compositor.render_frame(scene).has_value());
    CHECK(rig.display->flushes == flushes); // nothing on screen was dirty, so nothing was redrawn or presented
    // An extreme invalidation that does cover the screen redraws all of it.
    rig.compositor.invalidate_region({kMin, kMin, kHuge, kHuge});
    CHECK(rig.compositor.render_frame(scene).has_value());
    CHECK(rig.display->flushes == flushes + 1);
    rig.expect_matches_reference(scene);

    g_context = "guards after invalid inputs";
    CHECK(small->guards_intact() && wallpaper->guards_intact());
    check_heap();
    std::printf("test_zero_sized_and_invalid_inputs passed (%zu layer cases + invalidation cases)\n", std::size(cases));
}

/// Clipping must not turn into over-drawing: only dirty pixels are recomposited, even when the window is off-screen.
void test_dirty_regions_still_limit_redraw() {
    SmallRig rig;
    Window win;
    auto wallpaper = make_wallpaper();
    auto scene = win.scene_at(wallpaper, -12, -11);
    rig.compositor.invalidate_region({0, 0, SmallRig::kW, SmallRig::kH});
    CHECK(rig.compositor.render_frame(scene).has_value());
    const auto before = rig.display->last.pixels;

    // Change everything the scene shows, but invalidate only one small rectangle.
    paint(*wallpaper, 21);
    paint(*win.content, 22);
    const Rect dirty{40, 20, 6, 5};
    rig.compositor.invalidate_region(dirty);
    CHECK(rig.compositor.render_frame(scene).has_value());
    const auto expected_inside = reference_frame(scene, SmallRig::kW, SmallRig::kH);
    for (uint32_t y = 0; y < SmallRig::kH; ++y) {
        for (uint32_t x = 0; x < SmallRig::kW; ++x) {
            const size_t i = (static_cast<size_t>(y) * SmallRig::kW + x) * 4;
            const auto& want = inside(dirty, x, y) ? expected_inside : before;
            g_context = "dirty-limited redraw at x=" + std::to_string(x) + " y=" + std::to_string(y);
            CHECK(std::memcmp(&rig.display->last.pixels[i], &want[i], 4) == 0);
        }
    }
    std::printf("test_dirty_regions_still_limit_redraw passed\n");
}

// ---------------------------------------------------------------------------------------------------------------------
// The original crash scenario, end to end: the real shell, window manager and decorator, driven by mouse events
// ---------------------------------------------------------------------------------------------------------------------
struct DesktopRig {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<CapturingDisplay> display = std::make_shared<CapturingDisplay>();
    std::shared_ptr<BasicCompositor> compositor = std::make_shared<BasicCompositor>(display, logger);
    std::shared_ptr<BasicWindowManager> wm = std::make_shared<BasicWindowManager>(bus, logger);
    std::shared_ptr<BasicWidgetToolkit> toolkit = std::make_shared<BasicWidgetToolkit>();
    std::shared_ptr<BasicWindowDecorator> decorator = std::make_shared<BasicWindowDecorator>(toolkit);
    std::shared_ptr<BasicTaskbar> taskbar = std::make_shared<BasicTaskbar>(toolkit);
    std::unique_ptr<apps::DesktopShell> shell;
    uint32_t win{0};

    DesktopRig() {
        shell = std::make_unique<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
        shell->launch();
        win = wm->create_window("Mover", {300, 300, 400, 250});
        auto content = compositor->create_surface(400, 250);
        paint(*content, 7);
        wm->set_window_surface(win, content);
        frame();
    }
    void frame() {
        shell->update_frame();
        CHECK(compositor->render_frame(wm->build_scene()).has_value());
    }
    void mouse(int32_t x, int32_t y, uint8_t buttons) {
        auto ev = std::make_shared<core::Event>();
        ev->topic = hal::kTopicInputMouse;
        ev->payload = hal::MouseEvent(x, y, buttons);
        bus->dispatch_immediate(ev);
    }
    /// The incrementally composited frame must equal a full recomposition of the same scene: nothing stale is left
    /// behind at the edges and nothing outside the framebuffer was touched.
    void expect_incremental_equals_full() {
        const auto incremental = display->last.pixels;
        compositor->invalidate_region({0, 0, 1920, 1080});
        frame();
        const int64_t diff = first_difference(incremental, display->last.pixels);
        if (diff >= 0) g_context += " | first stale pixel x=" + std::to_string(diff % 1920) + " y=" + std::to_string(diff / 1920);
        CHECK(diff < 0);
    }
};

void test_shell_drag_beyond_every_edge() {
    DesktopRig r;
    const auto start = *r.wm->get_window_geometry(r.win);
    // Grab the middle of the title bar.
    const Rect deco = r.wm->get_decorated_window_bounds(r.win);
    const int32_t gx = deco.x + 150, gy = deco.y + 6 + 18;
    struct Target {
        const char* name;
        int32_t x, y; // client origin to reach
    };
    const Target targets[] = {
        {"left, partly off", -200, 300},        {"left, fully off", -1000, 300},
        {"top, partly off", 300, -100},         {"top, fully off", 300, -800},
        {"right, partly off", 1700, 300},       {"right, fully off", 2500, 300},
        {"bottom, partly off", 300, 900},       {"bottom, fully off", 300, 1500},
        {"top-left corner", -200, -100},        {"top-right corner", 1700, -100},
        {"bottom-left corner", -200, 900},      {"bottom-right corner", 1700, 900},
        {"far negative", -100000, -100000},     {"far positive", 100000, 100000},
        {"home", 300, 300},
    };
    r.mouse(gx, gy, 1);
    int32_t cur_x = start.x, cur_y = start.y;
    for (const Target& t : targets) {
        // Three steps per move, with pointer coordinates that go negative as they do under mouse capture.
        for (int step = 1; step <= 3; ++step) {
            const int32_t nx = cur_x + (t.x - cur_x) * step / 3, ny = cur_y + (t.y - cur_y) * step / 3;
            r.mouse(gx + (nx - start.x), gy + (ny - start.y), 1);
            r.frame();
            g_context = std::string("shell drag towards '") + t.name + "' step " + std::to_string(step);
            const auto g = *r.wm->get_window_geometry(r.win);
            CHECK(g.x == nx && g.y == ny); // movement is not restricted
            r.expect_incremental_equals_full();
        }
        cur_x = t.x;
        cur_y = t.y;
    }
    r.mouse(gx, gy, 0);
    r.frame();
    std::printf("test_shell_drag_beyond_every_edge passed (%zu targets x 3 steps)\n", std::size(targets));
}

void test_shell_resize_maximize_restore_after_off_screen() {
    DesktopRig r;
    // Park the window partly off the top-left, then bring it back and use it normally. A direct window-manager move does
    // not invalidate anything (the shell does that for the moves it makes), so invalidate the old and new frame like it does.
    auto move_to = [&](int32_t x, int32_t y) {
        r.compositor->invalidate_region(r.wm->get_decorated_window_bounds(r.win));
        r.wm->move_window(r.win, x, y);
        r.compositor->invalidate_region(r.wm->get_decorated_window_bounds(r.win));
        r.frame();
    };
    move_to(-150, -80);
    g_context = "parked off the top-left";
    r.expect_incremental_equals_full();
    move_to(300, 300);
    g_context = "back on screen";
    r.expect_incremental_equals_full();

    // Resize from the right edge.
    const Rect d = r.wm->get_decorated_window_bounds(r.win);
    const int32_t ex = d.x + static_cast<int32_t>(d.width) - 3, ey = d.y + static_cast<int32_t>(d.height / 2);
    r.mouse(ex, ey, 1);
    r.mouse(ex + 60, ey, 1);
    r.frame();
    r.mouse(ex + 60, ey, 0);
    r.frame();
    g_context = "resize from the right edge";
    CHECK(r.wm->get_window_geometry(r.win)->width == 460);
    r.expect_incremental_equals_full();

    // Maximize and restore with the frame's own button (acts on release).
    auto click_maximize = [&] {
        const Rect b = r.wm->get_decorated_window_bounds(r.win);
        const auto g = *r.wm->get_window_geometry(r.win);
        const auto l = calculate_decoration_layout(make_frame_metrics(r.wm->get_window_frame_style(r.win), b, {g.x, g.y, g.width, g.height}));
        const int32_t mx = b.x + l.maximize_button.x + static_cast<int32_t>(l.maximize_button.width / 2);
        const int32_t my = b.y + l.maximize_button.y + static_cast<int32_t>(l.maximize_button.height / 2);
        r.mouse(mx, my, 0);
        r.mouse(mx, my, 1);
        r.mouse(mx, my, 0);
        r.frame();
    };
    click_maximize();
    g_context = "maximize";
    CHECK(r.wm->get_window_state(r.win) == WindowState::Maximized);
    r.expect_incremental_equals_full();
    click_maximize();
    g_context = "restore";
    CHECK(r.wm->get_window_state(r.win) == WindowState::Normal);
    CHECK(r.wm->get_window_geometry(r.win)->width == 460);
    r.expect_incremental_equals_full();
    std::printf("test_shell_resize_maximize_restore_after_off_screen passed\n");
}

} // namespace

int main() {
#if defined(_MSC_VER)
    _set_error_mode(_OUT_TO_STDERR);
#endif
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("Running compositor bounds tests...\n");
    test_edges_and_corners();
    test_exhaustive_drag_sweep();
    test_zero_sized_and_invalid_inputs();
    test_dirty_regions_still_limit_redraw();
    test_shell_drag_beyond_every_edge();
    test_shell_resize_maximize_restore_after_off_screen();
    check_heap();
    std::printf("All compositor bounds tests passed.\n");
    return 0;
}
