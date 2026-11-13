#include "scene_snapshot.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace vynexos::bootstrap {

namespace {

const char* layer_name(desktop::WindowLayer layer) {
    switch (layer) {
    case desktop::WindowLayer::Desktop: return "Desktop";
    case desktop::WindowLayer::Normal: return "Normal";
    case desktop::WindowLayer::Panel: return "Panel";
    case desktop::WindowLayer::Overlay: return "Overlay";
    }
    return "Unknown";
}

const char* state_name(desktop::WindowState state) {
    switch (state) {
    case desktop::WindowState::Normal: return "Normal";
    case desktop::WindowState::Minimized: return "Minimized";
    case desktop::WindowState::Maximized: return "Maximized";
    }
    return "Unknown";
}

/// Titles are written on one line: line breaks become spaces.
std::string one_line(std::string text) {
    std::replace_if(text.begin(), text.end(), [](char c) { return c == '\n' || c == '\r'; }, ' ');
    return text;
}

} // namespace

std::string format_scene_snapshot(const desktop::IWindowManager& wm, const desktop::SceneGraph& scene) {
    std::unordered_set<uint32_t> in_scene;
    for (const auto& layer : scene) in_scene.insert(layer.window_id);

    // Title, state and the window's own visibility flag (shown or hidden, independent of its state; is_window_visible()
    // would also report a minimized or maximized window as not visible).
    std::unordered_map<uint32_t, desktop::WindowSnapshot> records;
    for (auto& record : wm.get_window_snapshots()) records.emplace(record.window_id, std::move(record));

    std::string out = "vynexos-scene-snapshot 1\n";
    const auto active = wm.get_active_window();
    out += active ? std::format("active {}\n", *active) : std::string("active none\n");
    for (const uint32_t id : wm.get_windows_z_ordered()) {
        const auto geometry = wm.get_window_geometry(id);
        const auto record = records.find(id);
        if (!geometry || record == records.end()) continue; // no longer registered
        out += std::format("window id={} layer={} state={} visible={} in_scene={} x={} y={} w={} h={} title={}\n", id,
                           layer_name(wm.get_window_layer(id)), state_name(record->second.state),
                           record->second.is_visible ? 1 : 0, in_scene.contains(id) ? 1 : 0, geometry->x, geometry->y,
                           geometry->width, geometry->height, one_line(record->second.title));
    }
    out += "end\n";
    return out;
}

SceneSnapshotWriter::SceneSnapshotWriter(std::filesystem::path path, std::shared_ptr<core::ILogger> logger)
    : m_path(std::move(path)), m_logger(std::move(logger)) {
    m_temp_path = m_path;
    m_temp_path += ".tmp";
}

bool SceneSnapshotWriter::update(const desktop::IWindowManager& wm, const desktop::SceneGraph& scene) {
    std::string text = format_scene_snapshot(wm, scene);
    if (m_has_written && text == m_last_written) return false;
    if (!write(text)) return false;
    m_last_written = std::move(text);
    m_has_written = true;
    ++m_writes;
    return true;
}

bool SceneSnapshotWriter::write(const std::string& text) {
    {
        std::ofstream file(m_temp_path, std::ios::binary | std::ios::trunc);
        if (file) file.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!file) {
            if (!m_failing && m_logger) m_logger->warning("SceneSnapshot: cannot write '{}'", m_temp_path.string());
            m_failing = true;
            return false;
        }
    }
    // Replaces the previous snapshot in one step. It fails while a reader holds the file open without delete sharing;
    // the next frame retries.
    std::error_code ec;
    std::filesystem::rename(m_temp_path, m_path, ec);
    if (ec) {
        if (!m_failing && m_logger) m_logger->warning("SceneSnapshot: cannot replace '{}': {}", m_path.string(), ec.message());
        m_failing = true;
        return false;
    }
    m_failing = false;
    return true;
}

} // namespace vynexos::bootstrap
