#pragma once
#include "vynexos/core/virtual_file_system.hpp"
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace vynexos::core {

/// One directory or file to be present in a filesystem. Paths are absolute VFS paths ("/docs/a.txt").
struct SeedEntry {
    std::string path;
    bool is_directory{false};
    std::string contents; ///< files only: the bytes written to a newly created file (empty: an empty file)

    [[nodiscard]] static SeedEntry directory(std::string path) { return {std::move(path), true, {}}; }
    [[nodiscard]] static SeedEntry file(std::string path, std::string contents = {}) { return {std::move(path), false, std::move(contents)}; }
};

struct SeedFailure {
    std::string path;
    std::string reason;
};

struct SeedResult {
    std::size_t created{0}; ///< entries that did not exist and now do
    std::size_t skipped{0}; ///< entries already present (of the same kind) and left exactly as they were
    std::vector<SeedFailure> failures;
    [[nodiscard]] bool ok() const noexcept { return failures.empty(); }
};

/**
 * @brief Makes sure the listed entries exist, using nothing but the IVirtualFileSystem interface.
 *
 * Entries are processed in order, so a directory is listed before what it contains. Existing user data is never touched:
 *  - an entry that already exists as the same kind is skipped (a file keeps its contents, a directory its children);
 *  - an entry that exists as the other kind (a file where a directory is wanted, or the reverse) is a failure;
 *  - a missing parent directory is a failure (parents are not created implicitly).
 * A failure never stops the run: every other entry is still processed, and every failure is reported with its path and
 * the reason. A file that cannot be written completely is removed again, so no half-written file is left behind.
 * Seeding is idempotent: running the same list twice creates nothing the second time.
 */
[[nodiscard]] SeedResult seed_filesystem(IVirtualFileSystem& vfs, std::span<const SeedEntry> entries);

} // namespace vynexos::core
