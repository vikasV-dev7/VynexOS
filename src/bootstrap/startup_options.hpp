#pragma once
#include "vfs_profiles.hpp"
#include <span>
#include <string>
#include <string_view>

namespace vynexos::bootstrap {

/// How the system starts. The defaults are what a plain launch (Visual Studio, the command line, a double click) uses.
struct StartupOptions {
    VfsProfile vfs_profile{VfsProfile::Development}; ///< the default while the project is pre-release
    /// Test and diagnostic interface: when set, the desktop's window table is published to this file whenever it
    /// changes (see scene_snapshot.hpp). Empty (the default): nothing is written.
    std::string scene_snapshot_path;
};

struct StartupParse {
    StartupOptions options;
    std::string error; ///< empty on success, otherwise a one-line diagnostic
    [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

/// Parses the arguments after the program name. Supported:  --vfs-profile=empty | --vfs-profile=development,
/// --scene-snapshot=<file>
/// A repeated option takes its last value. An unknown profile name, an option without a value, and any argument that is
/// not an option above are errors (nothing is guessed, so a typo never silently starts the wrong configuration).
[[nodiscard]] StartupParse parse_startup_options(std::span<const std::string_view> args);
/// As above, from main()'s argc/argv (argv[0] is skipped).
[[nodiscard]] StartupParse parse_startup_options(int argc, const char* const* argv);
/// The command-line summary printed after a diagnostic.
[[nodiscard]] std::string startup_usage();

} // namespace vynexos::bootstrap
