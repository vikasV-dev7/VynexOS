// Surface allocation limits (surface_limits.hpp) and what happens when a size is refused: nothing is allocated, nothing
// overflows, and whoever asked keeps the valid surface it already had.
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../apps/application_host.hpp"
#include "../apps/desktop_shell.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_taskbar.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/software_surface.hpp"
#include "vynexos/apps/application_registry.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/desktop/surface_limits.hpp"

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
    explicit CapturingDisplay(hal::DisplaySize size = hal::kDefaultDisplaySize) : m_size(size) {}
    std::expected<void, DisplayError> initialize() override { return {}; }
    std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) override {
        last = buffer;
        return {};
    }
    void shutdown() override {}
    hal::DisplaySize logical_size() const override { return m_size; }
    hal::FrameBuffer last{0, 0, {}};

private:
    hal::DisplaySize m_size;
};

constexpr uint32_t kU32Max = std::numeric_limits<uint32_t>::max();

// ---------------------------------------------------------------------------------------------------------------------
void test_byte_size_limits() {
    vynex_test::context() = "valid sizes";
    CHECK(surface_byte_size(1, 1) == std::optional<std::size_t>{4});
    CHECK(surface_byte_size(1920, 1080) == std::optional<std::size_t>{std::size_t{1920} * 1080 * 4});
    CHECK(surface_byte_size(kMaxSurfaceDimension, 1).has_value());
    CHECK(surface_byte_size(1, kMaxSurfaceDimension).has_value());
    CHECK(surface_byte_size(8192, 8192) == std::optional<std::size_t>{std::size_t{256} * 1024 * 1024}); // exactly the pixel limit
    CHECK(surface_byte_size(7680, 4320).has_value()); // an 8K desktop

    vynex_test::context() = "zero, too wide or tall, too many pixels, overflow-sized";
    CHECK(!surface_byte_size(0, 0) && !surface_byte_size(0, 100) && !surface_byte_size(100, 0));
    CHECK(!surface_byte_size(kMaxSurfaceDimension + 1, 1) && !surface_byte_size(1, kMaxSurfaceDimension + 1));
    CHECK(!surface_byte_size(8192, 8193));                                  // one row past the pixel limit
    CHECK(!surface_byte_size(kMaxSurfaceDimension, kMaxSurfaceDimension)); // each side allowed, together 1 GiB: refused
    CHECK(!surface_byte_size(kU32Max, kU32Max));                            // 32-bit w*h*4 would wrap; refused
    CHECK(!surface_byte_size(65536, 65536));                                // w*h wraps to 0 in 32 bits
    CHECK(!surface_byte_size(32768, 32768));                                // w*h*4 wraps to 0 in 32 bits
    std::printf("test_byte_size_limits passed\n");
}

void test_allocation_requests() {
    auto logger = std::make_shared<TestLogger>();
    BasicCompositor compositor(std::make_shared<CapturingDisplay>(), logger);

    vynex_test::context() = "the compositor refuses before allocating";
    for (const auto& [w, h] : std::vector<std::pair<uint32_t, uint32_t>>{{0, 0}, {0, 10}, {10, 0}, {kMaxSurfaceDimension + 1, 10},
                                                                         {8193, 8192}, {65536, 65536}, {kU32Max, kU32Max}, {kU32Max, 1}}) {
        CHECK(compositor.create_surface(w, h) == nullptr);
    }
    const auto ok = compositor.create_surface(640, 480);
    CHECK(ok && ok->width() == 640 && ok->height() == 480 && ok->map_pixels().size() == std::size_t{640} * 480 * 4);
    const auto thin = compositor.create_surface(kMaxSurfaceDimension, 2);
    CHECK(thin && thin->map_pixels().size() == std::size_t{kMaxSurfaceDimension} * 2 * 4);

    vynex_test::context() = "a SoftwareSurface built directly throws instead of overflowing";
    for (const auto& [w, h] : std::vector<std::pair<uint32_t, uint32_t>>{{kMaxSurfaceDimension + 1, 1}, {65536, 65536}, {kU32Max, kU32Max}}) {
        bool threw = false;
        try {
            SoftwareSurface s(w, h);
        } catch (const std::length_error&) {
            threw = true;
        }
        CHECK(threw);
    }
    SoftwareSurface empty(0, 0); // empty is allowed and owns nothing
    CHECK(empty.map_pixels().empty() && empty.width() == 0);
    SoftwareSurface empty_wide(5000, 0);
    CHECK(empty_wide.map_pixels().empty());

    vynex_test::context() = "a display size no surface may have: the compositor reports an error, it does not overflow";
    BasicCompositor huge(std::make_shared<CapturingDisplay>(hal::DisplaySize{70000, 70000}), logger);
    const auto rendered = huge.render_frame({});
    CHECK(!rendered.has_value() && rendered.error() == DisplayError::InvalidBuffer);
    std::printf("test_allocation_requests passed\n");
}

/// Counts renders and the surface size it rendered into.
class ProbeApp final : public apps::IApplication {
public:
    int renders{0};
    uint32_t rendered_w{0}, rendered_h{0};
    bool report{false};
    bool initialize() override { return true; }
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event&) override { return false; }
    void render(ISurface& s) override {
        ++renders;
        rendered_w = s.width();
        rendered_h = s.height();
        auto px = s.map_pixels();
        for (size_t i = 0; i < px.size(); i += 4) px[i] = 200, px[i + 1] = static_cast<uint8_t>(renders), px[i + 2] = 50, px[i + 3] = 255;
        s.unmap_pixels();
    }
    void shutdown() override { ++shutdowns; }
    std::optional<Rect> take_invalidated_region() override { return std::exchange(report, false) ? std::optional<Rect>{apps::kWholeContent} : std::nullopt; }
    [[nodiscard]] std::string get_name() const noexcept override { return "Probe"; }
    static inline int shutdowns{0};
};

struct ShellRig {
    std::shared_ptr<TestLogger> logger = std::make_shared<TestLogger>();
    core::BasicTaskScheduler scheduler{*logger, 2};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<CapturingDisplay> display = std::make_shared<CapturingDisplay>();
    std::shared_ptr<BasicCompositor> compositor = std::make_shared<BasicCompositor>(display, logger);
    std::shared_ptr<BasicWindowManager> wm = std::make_shared<BasicWindowManager>(bus, logger);
    std::shared_ptr<BasicWidgetToolkit> toolkit = std::make_shared<BasicWidgetToolkit>();
    std::shared_ptr<apps::ApplicationRegistry> registry = std::make_shared<apps::ApplicationRegistry>();
    std::shared_ptr<apps::DesktopShell> shell;
    std::shared_ptr<apps::ApplicationHost> host;

    ShellRig() {
        shell = std::make_shared<apps::DesktopShell>(wm, compositor, toolkit, std::make_shared<BasicWindowDecorator>(toolkit),
                                                     std::make_shared<BasicTaskbar>(toolkit), bus);
        host = std::make_shared<apps::ApplicationHost>(registry, wm, compositor, shell);
        shell->set_application_host(host.get());
        auto probe = [] { return std::make_unique<ProbeApp>(); };
        host->register_application({"Probe", "Probe", {300, 200, 400, 300}}, probe);
        host->register_application({"Empty", "Empty", {300, 200, 0, 0}}, probe);
        host->register_application({"Huge", "Huge", {0, 0, 9000, 9000}}, probe);
        shell->launch();
        settle();
    }
    ~ShellRig() {
        host->close_all();
        shell->set_application_host(nullptr);
    }
    void settle() {
        for (int i = 0; i < 3; ++i) {
            shell->update_frame();
            CHECK(compositor->render_frame(wm->build_scene()).has_value());
        }
    }
    /// The content surface and the decoration surface the window manager composes for a window.
    std::pair<std::shared_ptr<ISurface>, std::shared_ptr<ISurface>> surfaces_of(uint32_t window) const {
        std::shared_ptr<ISurface> decoration, content;
        for (const auto& layer : wm->build_scene()) {
            if (layer.window_id != window) continue;
            if (!decoration) decoration = layer.surface;
            else content = layer.surface;
        }
        return {content, decoration};
    }
};

void test_failed_resize_keeps_the_valid_surface() {
    ShellRig r;
    const uint32_t w = *r.host->open("Probe");
    r.settle();
    auto* app = dynamic_cast<ProbeApp*>(r.host->application_in(w));
    CHECK(app != nullptr);
    const auto [content, decoration] = r.surfaces_of(w);
    CHECK(content && content->width() == 400 && content->height() == 300 && decoration);
    const int renders = app->renders;

    vynex_test::context() = "a client area whose surface the limits refuse (9000 x 9000 = 81 Mi pixels)";
    r.wm->set_window_geometry(w, {100, 100, 9000, 9000});
    r.settle();
    const auto [content_after, decoration_after] = r.surfaces_of(w);
    CHECK(content_after == content && content_after->width() == 400 && content_after->height() == 300); // the same, intact surface
    CHECK(decoration_after == decoration);
    CHECK(content->map_pixels().size() == std::size_t{400} * 300 * 4);
    // ...and it is still in use: a reported change is rendered into it and reaches the screen.
    app->report = true;
    r.settle();
    CHECK(app->renders == renders + 1 && app->rendered_w == 400 && app->rendered_h == 300);
    const auto& fb = r.display->last;
    const size_t i = (static_cast<size_t>(100 + 10) * fb.width + 100 + 10) * 4; // client origin (100,100) + (10,10)
    auto px = content->map_pixels();
    const size_t s = (static_cast<size_t>(10) * 400 + 10) * 4;
    CHECK(fb.pixels[i] == px[s] && fb.pixels[i + 1] == px[s + 1] && fb.pixels[i + 2] == px[s + 2]);
    content->unmap_pixels();

    vynex_test::context() = "back to a valid size: a new surface of that size, painted";
    r.wm->set_window_geometry(w, {100, 100, 500, 350});
    r.settle();
    const auto [resized, redecorated] = r.surfaces_of(w);
    CHECK(resized && resized != content && resized->width() == 500 && resized->height() == 350);
    CHECK(redecorated && redecorated != decoration);
    CHECK(app->rendered_w == 500 && app->rendered_h == 350);
    std::printf("test_failed_resize_keeps_the_valid_surface passed\n");
}

void test_host_refuses_windows_it_cannot_allocate() {
    ShellRig r;
    const size_t windows = r.wm->get_window_snapshots().size();
    const int shutdowns = ProbeApp::shutdowns;
    for (const char* id : {"Empty", "Huge"}) {
        vynex_test::context() = std::string("opening ") + id;
        CHECK(!r.host->open(id).has_value());
        CHECK(!r.host->window_of(id).has_value());
        CHECK(r.wm->get_window_snapshots().size() == windows); // no window left behind
        CHECK(r.registry->get_running_applications().empty()); // the instance was terminated
    }
    CHECK(ProbeApp::shutdowns == shutdowns + 2);
    CHECK(r.host->open("Probe").has_value()); // a valid one still opens
    std::printf("test_host_refuses_windows_it_cannot_allocate passed\n");
}

} // namespace

int main() {
    std::printf("Running surface limit tests...\n");
    test_byte_size_limits();
    test_allocation_requests();
    test_failed_resize_keeps_the_valid_surface();
    test_host_refuses_windows_it_cannot_allocate();
    std::printf("All surface limit tests passed.\n");
    return 0;
}
