#include "vynexos/core/vfs_seed.hpp"
#include <algorithm>
#include <cstdint>

namespace vynexos::core {

namespace {

// "/a/b" -> "/a", "/a" -> "/".
std::string parent_of(const std::string& path) {
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) return "/";
    return path.substr(0, slash);
}

const char* kind_name(bool is_directory) {
    return is_directory ? "directory" : "file";
}

// Returns an empty string on success, otherwise the reason the entry could not be created.
std::string create_entry(IVirtualFileSystem& vfs, const SeedEntry& entry) {
    const std::string parent = parent_of(entry.path);
    const auto parent_stat = vfs.stat(parent);
    if (!parent_stat) return "parent directory " + parent + " does not exist";
    if (!parent_stat->is_directory) return "parent " + parent + " is not a directory";

    if (entry.is_directory) {
        if (!vfs.create_directory(entry.path)) return "the filesystem refused to create the directory";
        return {};
    }

    std::unique_ptr<IFileHandle> handle = vfs.create(entry.path);
    if (!handle) return "the filesystem refused to create the file";
    if (entry.contents.empty()) return {};

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(entry.contents.data());
    const std::size_t written = handle->write(std::span<const std::uint8_t>(bytes, entry.contents.size()));
    if (written == entry.contents.size()) {
        handle->flush();
        return {};
    }
    handle.reset();
    (void)vfs.remove(entry.path); // never leave a half-written file behind
    return "wrote " + std::to_string(written) + " of " + std::to_string(entry.contents.size()) + " bytes";
}

} // namespace

SeedResult seed_filesystem(IVirtualFileSystem& vfs, std::span<const SeedEntry> entries) {
    SeedResult result;
    for (const SeedEntry& entry : entries) {
        if (entry.path.empty() || entry.path.front() != '/') {
            result.failures.push_back({entry.path, "the path is not absolute"});
            continue;
        }

        // Look before creating: IVirtualFileSystem::create() truncates an existing file, which would destroy user data.
        if (const auto existing = vfs.stat(entry.path)) {
            if (existing->is_directory == entry.is_directory) {
                ++result.skipped;
            } else {
                result.failures.push_back({entry.path, std::string("already exists as a ") + kind_name(existing->is_directory) + ", a " +
                                                           kind_name(entry.is_directory) + " was wanted"});
            }
            continue;
        }

        std::string reason = create_entry(vfs, entry);
        if (reason.empty()) ++result.created;
        else result.failures.push_back({entry.path, std::move(reason)});
    }
    return result;
}

} // namespace vynexos::core
