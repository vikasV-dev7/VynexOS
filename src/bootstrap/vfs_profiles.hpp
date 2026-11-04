#pragma once
#include "vynexos/core/vfs_seed.hpp"
#include <optional>
#include <span>
#include <string_view>

namespace vynexos::bootstrap {

/// What a freshly started system's filesystem contains. The filesystem itself knows nothing about profiles: the startup
/// layer chooses one and applies it through the IVirtualFileSystem interface (see core::seed_filesystem).
enum class VfsProfile {
    Empty,       ///< nothing
    Development, ///< a small, fixed set of folders and text files for working on the desktop
};

[[nodiscard]] std::string_view to_string(VfsProfile profile) noexcept;
/// The profile with this exact (lower-case) name, or nothing.
[[nodiscard]] std::optional<VfsProfile> parse_vfs_profile(std::string_view name) noexcept;
/// The profile's entries, in creation order. Deterministic: the same on every call and every run.
[[nodiscard]] std::span<const core::SeedEntry> vfs_profile_entries(VfsProfile profile);
/// Seeds `vfs` with the profile. Existing entries are left alone; see core::seed_filesystem.
[[nodiscard]] core::SeedResult apply_vfs_profile(core::IVirtualFileSystem& vfs, VfsProfile profile);

} // namespace vynexos::bootstrap
