#pragma once
// Scene snapshot: a test and diagnostic interface that publishes the desktop's window table to a file, so external
// tools (the UI harness) can read window positions, states and scene membership without parsing the runtime log.
//
// Enabled only by the --scene-snapshot=<path> startup option; a normal launch never creates it. The file is rewritten
// only when its content changes (an idle desktop writes nothing), and each write replaces the file whole (written to a
// temporary file, then renamed), so a reader never sees a half-written snapshot.
//
// Format (UTF-8 text, one record per line; the title is last so it may contain spaces):
//   vynexos-scene-snapshot 1
//   active <window id>|none
//   window id=<n> layer=<Desktop|Normal|Panel|Overlay> state=<Normal|Minimized|Maximized> visible=<0|1> in_scene=<0|1>
//          x=<n> y=<n> w=<n> h=<n> title=<text>          (all on one line; windows bottom to top, in stacking order)
//   end
// x, y, w, h are the window's client geometry in logical desktop coordinates. visible is the window's own shown/hidden
// flag, independent of its state (a minimized window stays visible=1). in_scene says whether the window was composed in
// the frame the snapshot describes.
#include <filesystem>
#include <memory>
#include <string>

#include "vynexos/core/logger.hpp"
#include "vynexos/desktop/desktop_types.hpp"
#include "vynexos/desktop/window_manager.hpp"

namespace vynexos::bootstrap {

/// The snapshot text for a window manager and the scene it just built.
[[nodiscard]] std::string format_scene_snapshot(const desktop::IWindowManager& wm, const desktop::SceneGraph& scene);

class SceneSnapshotWriter {
public:
    SceneSnapshotWriter(std::filesystem::path path, std::shared_ptr<core::ILogger> logger);

    /// Writes the snapshot when it differs from the last one written. Returns true when the file was written.
    /// A failed write is retried on the next call; it is logged once per run of failures, not once per frame.
    bool update(const desktop::IWindowManager& wm, const desktop::SceneGraph& scene);

    /// How many times the file has been written (diagnostics and tests).
    [[nodiscard]] std::size_t write_count() const noexcept { return m_writes; }

private:
    bool write(const std::string& text);

    std::filesystem::path m_path;
    std::filesystem::path m_temp_path;
    std::shared_ptr<core::ILogger> m_logger;
    std::string m_last_written;
    bool m_has_written{false};
    bool m_failing{false};
    std::size_t m_writes{0};
};

} // namespace vynexos::bootstrap
