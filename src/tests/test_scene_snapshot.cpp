// Idle logging and the scene snapshot that replaces the periodic scene dump.
//  - Building the scene (every frame) writes nothing to the log, however many frames run.
//  - Warnings and errors are still logged.
//  - The scene snapshot describes every window (position, state, layer, visibility, scene membership) from the public
//    window manager interface, and its writer touches the file only when the content changes.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../bootstrap/scene_snapshot.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/mock_display_backend.hpp"
#include "vynexos/core/logger.hpp"

#include "test_check.hpp"

using namespace vynexos;

namespace {

class CountingLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Trace; } // nothing filtered
    void log_raw(core::LogLevel level, std::string_view message) override {
        ++lines;
        if (level == core::LogLevel::Warning) ++warnings;
        if (level >= core::LogLevel::Error) {
            ++errors;
            last_error = std::string(message);
        }
        last = std::string(message);
    }
    std::atomic<int> lines{0}, warnings{0}, errors{0};
    std::string last;       // the most recent message of any level
    std::string last_error; // the most recent error
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

/// A window manager with the windows a desktop has: wallpaper, panel, a hidden overlay, a normal and a minimized
/// application window, and one window that has no surface yet.
struct Desktop {
    std::shared_ptr<CountingLogger> logger = std::make_shared<CountingLogger>();
    core::BasicTaskScheduler scheduler{*logger, 1};
    std::shared_ptr<core::InMemoryEventBus> bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    std::shared_ptr<desktop::MockDisplayBackend> display = std::make_shared<desktop::MockDisplayBackend>(logger);
    std::shared_ptr<desktop::BasicCompositor> compositor = std::make_shared<desktop::BasicCompositor>(display, logger);
    std::shared_ptr<desktop::BasicWindowManager> wm = std::make_shared<desktop::BasicWindowManager>(bus, logger);
    uint32_t wallpaper = 0, panel = 0, overlay = 0, app = 0, minimized = 0, bare = 0;

    uint32_t add(const char* title, desktop::WindowGeometry g, desktop::WindowLayer layer, bool surface = true) {
        const uint32_t id = wm->create_window(title, g);
        wm->set_window_layer(id, layer);
        if (surface) wm->set_window_surface(id, compositor->create_surface(g.width, g.height));
        return id;
    }

    Desktop() {
        wallpaper = add("Wallpaper", {0, 0, 1920, 1080}, desktop::WindowLayer::Desktop);
        panel = add("Taskbar", {0, 1032, 1920, 48}, desktop::WindowLayer::Panel);
        overlay = add("Launcher", {0, 600, 400, 432}, desktop::WindowLayer::Overlay);
        wm->set_window_visibility(overlay, false);
        app = add("File Explorer", {200, 200, 700, 500}, desktop::WindowLayer::Normal);
        minimized = add("Terminal", {120, 140, 640, 400}, desktop::WindowLayer::Normal);
        wm->set_window_state(minimized, desktop::WindowState::Minimized);
        bare = add("Pending", {50, 60, 300, 200}, desktop::WindowLayer::Normal, false);
        wm->focus_window(app);
    }
};

void test_building_the_scene_logs_nothing() {
    Desktop d;
    const int before = d.logger->lines;
    for (int frame = 0; frame < 1000; ++frame) {
        const auto scene = d.wm->build_scene();
        CHECK(!scene.empty());
    }
    // The old periodic dump wrote a table every 60 frames: 16 dumps of ~25 lines here.
    CHECK(d.logger->lines == before);
    std::printf("test_building_the_scene_logs_nothing passed (1000 frames, 0 log lines)\n");
}

void test_warnings_and_errors_remain_observable() {
    Desktop d;
    // A rejected window size is a warning.
    const int warnings = d.logger->warnings;
    CHECK(!d.wm->resize_window(d.app, 100000, 100));
    CHECK(d.logger->warnings == warnings + 1);
    CHECK(d.logger->last.find("Rejected size") != std::string::npos);

    // A failing background task is an error.
    CountingLogger scheduler_log;
    {
        core::BasicTaskScheduler scheduler(scheduler_log, 1);
        CHECK(scheduler.enqueue([](const core::ExecutionContext&) { throw std::runtime_error("boom"); }));
    } // destruction drains the queue
    CHECK(scheduler_log.errors == 1);
    CHECK(scheduler_log.last_error.find("boom") != std::string::npos);
    std::printf("test_warnings_and_errors_remain_observable passed\n");
}

void test_snapshot_describes_every_window() {
    Desktop d;
    const auto text = bootstrap::format_scene_snapshot(*d.wm, d.wm->build_scene());
    std::vector<std::string> lines;
    for (std::istringstream in(text); !in.eof();) {
        std::string line;
        std::getline(in, line);
        if (!line.empty()) lines.push_back(line);
    }
    // Header, the focused window, one record per window bottom to top (layer order, then stacking), terminator.
    const std::vector<std::string> expected = {
        "vynexos-scene-snapshot 1",
        "active " + std::to_string(d.app),
        "window id=" + std::to_string(d.wallpaper) + " layer=Desktop state=Normal visible=1 in_scene=1 x=0 y=0 w=1920 h=1080 title=Wallpaper",
        "window id=" + std::to_string(d.minimized) + " layer=Normal state=Minimized visible=1 in_scene=0 x=120 y=140 w=640 h=400 title=Terminal",
        "window id=" + std::to_string(d.bare) + " layer=Normal state=Normal visible=1 in_scene=0 x=50 y=60 w=300 h=200 title=Pending",
        "window id=" + std::to_string(d.app) + " layer=Normal state=Normal visible=1 in_scene=1 x=200 y=200 w=700 h=500 title=File Explorer",
        "window id=" + std::to_string(d.panel) + " layer=Panel state=Normal visible=1 in_scene=1 x=0 y=1032 w=1920 h=48 title=Taskbar",
        "window id=" + std::to_string(d.overlay) + " layer=Overlay state=Normal visible=0 in_scene=0 x=0 y=600 w=400 h=432 title=Launcher",
        "end",
    };
    if (lines != expected) {
        std::fprintf(stderr, "snapshot was:\n%s\n", text.c_str());
    }
    CHECK(lines == expected);
    std::printf("test_snapshot_describes_every_window passed\n");
}

void test_writer_writes_only_on_change() {
    Desktop d;
    const auto dir = std::filesystem::temp_directory_path() / ("vynex_scene_snapshot_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const auto path = dir / "scene_snapshot.txt";
    bootstrap::SceneSnapshotWriter writer(path, d.logger);

    CHECK(writer.update(*d.wm, d.wm->build_scene())); // first frame: written
    CHECK(read_file(path) == bootstrap::format_scene_snapshot(*d.wm, d.wm->build_scene()));
    for (int frame = 0; frame < 600; ++frame) CHECK(!writer.update(*d.wm, d.wm->build_scene())); // idle: untouched
    CHECK(writer.write_count() == 1);

    d.wm->move_window(d.app, 260, 240); // something changed: written once
    CHECK(writer.update(*d.wm, d.wm->build_scene()));
    CHECK(!writer.update(*d.wm, d.wm->build_scene()));
    CHECK(writer.write_count() == 2);
    CHECK(read_file(path).find("x=260 y=240 w=700 h=500 title=File Explorer") != std::string::npos);

    d.wm->set_window_state(d.minimized, desktop::WindowState::Normal); // restoring brings it into the scene
    CHECK(writer.update(*d.wm, d.wm->build_scene()));
    CHECK(read_file(path).find("state=Normal visible=1 in_scene=1 x=120 y=140 w=640 h=400 title=Terminal") != std::string::npos);
    CHECK(!std::filesystem::exists(dir / "scene_snapshot.txt.tmp")); // replaced whole; no leftover
    CHECK(d.logger->warnings == 0);
    std::filesystem::remove_all(dir);
    std::printf("test_writer_writes_only_on_change passed\n");
}

void test_writer_failure_is_logged_once() {
    Desktop d;
    const auto path = std::filesystem::temp_directory_path() / "vynex_no_such_directory_for_snapshot" / "missing" / "scene.txt";
    bootstrap::SceneSnapshotWriter writer(path, d.logger);
    const int warnings = d.logger->warnings;
    for (int frame = 0; frame < 300; ++frame) CHECK(!writer.update(*d.wm, d.wm->build_scene()));
    CHECK(writer.write_count() == 0);
    CHECK(d.logger->warnings == warnings + 1); // observable, but not once per frame
    std::printf("test_writer_failure_is_logged_once passed\n");
}

} // namespace

int main() {
    std::printf("Running scene snapshot and idle logging tests...\n");
    test_building_the_scene_logs_nothing();
    test_warnings_and_errors_remain_observable();
    test_snapshot_describes_every_window();
    test_writer_writes_only_on_change();
    test_writer_failure_is_logged_once();
    std::printf("All scene snapshot and idle logging tests passed.\n");
    return 0;
}
