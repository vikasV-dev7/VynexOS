#include "vfs_profiles.hpp"
#include <vector>

namespace vynexos::bootstrap {

namespace {

using core::SeedEntry;

// The development profile. File Explorer lists directories first, then names, so the root reads:
//   docs/  music/  pictures/  notes.md  readme.txt  todo.txt
// GUI checks rely on this order and on these contents; the profile tests pin both.
const std::vector<SeedEntry>& development_entries() {
    static const std::vector<SeedEntry> entries = {
        SeedEntry::directory("/docs"),
        SeedEntry::directory("/music"),
        SeedEntry::directory("/pictures"),
        SeedEntry::file("/notes.md",
                        "# Notes\n"
                        "\n"
                        "This is the VynexOS development filesystem.\n"
                        "It lives in memory and is recreated on every start.\n"),
        SeedEntry::file("/readme.txt",
                        "VynexOS development profile\n"
                        "---------------------------\n"
                        "The system seeds this filesystem at startup.\n"
                        "Start with --vfs-profile=empty for an empty one.\n"),
        SeedEntry::file("/todo.txt",
                        "- Open File Explorer from the launcher\n"
                        "- Browse docs/\n"
                        "- Try the Terminal\n"),
        SeedEntry::file("/docs/getting-started.md",
                        "# Getting started\n"
                        "\n"
                        "Open the launcher with the Vynex button to start the Terminal, File Explorer or the Demo App.\n"),
    };
    return entries;
}

} // namespace

std::string_view to_string(VfsProfile profile) noexcept {
    switch (profile) {
    case VfsProfile::Empty: return "empty";
    case VfsProfile::Development: return "development";
    }
    return "unknown";
}

std::optional<VfsProfile> parse_vfs_profile(std::string_view name) noexcept {
    if (name == "empty") return VfsProfile::Empty;
    if (name == "development") return VfsProfile::Development;
    return std::nullopt;
}

std::span<const core::SeedEntry> vfs_profile_entries(VfsProfile profile) {
    switch (profile) {
    case VfsProfile::Empty: return {};
    case VfsProfile::Development: return development_entries();
    }
    return {};
}

core::SeedResult apply_vfs_profile(core::IVirtualFileSystem& vfs, VfsProfile profile) {
    return core::seed_filesystem(vfs, vfs_profile_entries(profile));
}

} // namespace vynexos::bootstrap
