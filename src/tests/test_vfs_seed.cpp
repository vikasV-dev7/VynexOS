// The generic seeding helper (core::seed_filesystem): what it creates, what it leaves alone, and how it reports failure.
// It only uses the IVirtualFileSystem interface, which the tests prove by also running it against a decorator that can
// refuse or shorten operations.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "vynexos/core/memory_file_system.hpp"
#include "vynexos/core/vfs_seed.hpp"

#include "test_check.hpp"

using namespace vynexos::core;

namespace {

std::string read_all(IVirtualFileSystem& vfs, const std::string& path) {
    auto handle = vfs.open(path);
    CHECK(handle != nullptr);
    std::vector<std::uint8_t> bytes(handle->size());
    const std::size_t got = handle->read(bytes);
    CHECK(got == bytes.size());
    return std::string(bytes.begin(), bytes.end());
}
void write_all(IVirtualFileSystem& vfs, const std::string& path, const std::string& text) {
    auto handle = vfs.create(path);
    CHECK(handle != nullptr);
    const auto* data = reinterpret_cast<const std::uint8_t*>(text.data());
    const std::size_t written = handle->write(std::span<const std::uint8_t>(data, text.size()));
    CHECK(written == text.size());
}
std::vector<std::string> sorted_list(const IVirtualFileSystem& vfs, const std::string& path) {
    auto names = vfs.list_directory(path);
    std::sort(names.begin(), names.end());
    return names;
}

/// A filesystem that forwards to a MemoryFileSystem but refuses chosen operations, or accepts only a few bytes of a write.
class FaultyFileSystem final : public IVirtualFileSystem {
public:
    std::set<std::string> refuse_directory;
    std::set<std::string> refuse_file;
    std::set<std::string> short_write; // writes to these files accept at most 3 bytes

    std::unique_ptr<IFileHandle> open(const std::string& p) override { return m_inner.open(p); }
    std::unique_ptr<IFileHandle> create(const std::string& p) override {
        if (refuse_file.contains(p)) return nullptr;
        auto handle = m_inner.create(p);
        if (handle && short_write.contains(p)) return std::make_unique<ShortHandle>(std::move(handle));
        return handle;
    }
    bool remove(const std::string& p) override { return m_inner.remove(p); }
    bool exists(const std::string& p) const noexcept override { return m_inner.exists(p); }
    std::optional<FileStat> stat(const std::string& p) const override { return m_inner.stat(p); }
    bool create_directory(const std::string& p) override { return !refuse_directory.contains(p) && m_inner.create_directory(p); }
    std::vector<std::string> list_directory(const std::string& p) const override { return m_inner.list_directory(p); }

private:
    class ShortHandle final : public IFileHandle {
    public:
        explicit ShortHandle(std::unique_ptr<IFileHandle> inner) : m_inner(std::move(inner)) {}
        std::size_t read(std::span<std::uint8_t> b) override { return m_inner->read(b); }
        std::size_t write(std::span<const std::uint8_t> b) override { return m_inner->write(b.first(std::min<std::size_t>(b.size(), 3))); }
        bool seek(std::size_t o) noexcept override { return m_inner->seek(o); }
        std::size_t position() const noexcept override { return m_inner->position(); }
        std::size_t size() const noexcept override { return m_inner->size(); }
        void flush() override { m_inner->flush(); }

    private:
        std::unique_ptr<IFileHandle> m_inner;
    };
    MemoryFileSystem m_inner;
};

const std::vector<SeedEntry> kEntries = {
    SeedEntry::directory("/docs"),
    SeedEntry::directory("/docs/sub"),
    SeedEntry::file("/docs/a.txt", "alpha\n"),
    SeedEntry::file("/docs/sub/b.txt", "bravo"),
    SeedEntry::file("/empty.txt"),
};

// ---------------------------------------------------------------------------------------------------------------------
void test_creates_directories_and_files_with_contents() {
    MemoryFileSystem vfs;
    const SeedResult r = seed_filesystem(vfs, kEntries);
    CHECK(r.ok());
    CHECK(r.created == 5 && r.skipped == 0 && r.failures.empty());
    CHECK((sorted_list(vfs, "/") == std::vector<std::string>{"docs", "empty.txt"}));
    CHECK((sorted_list(vfs, "/docs") == std::vector<std::string>{"a.txt", "sub"}));
    CHECK(vfs.stat("/docs")->is_directory && vfs.stat("/docs/sub")->is_directory);
    CHECK(!vfs.stat("/docs/a.txt")->is_directory && vfs.stat("/docs/a.txt")->size_bytes == 6);
    CHECK(read_all(vfs, "/docs/a.txt") == "alpha\n");
    CHECK(read_all(vfs, "/docs/sub/b.txt") == "bravo");
    CHECK(vfs.stat("/empty.txt")->size_bytes == 0);
    std::printf("test_creates_directories_and_files_with_contents passed\n");
}

void test_nothing_to_seed() {
    MemoryFileSystem vfs;
    const SeedResult r = seed_filesystem(vfs, {});
    CHECK(r.ok() && r.created == 0 && r.skipped == 0);
    CHECK(vfs.list_directory("/").empty());
    std::printf("test_nothing_to_seed passed\n");
}

void test_existing_entries_are_not_overwritten() {
    MemoryFileSystem vfs;
    CHECK(vfs.create_directory("/docs"));
    write_all(vfs, "/docs/user.txt", "my own data");
    write_all(vfs, "/docs/a.txt", "edited by the user");
    write_all(vfs, "/empty.txt", "not empty any more");

    const SeedResult r = seed_filesystem(vfs, kEntries);
    CHECK(r.ok());
    CHECK(r.created == 2 && r.skipped == 3); // /docs/sub and /docs/sub/b.txt are new; /docs, a.txt, empty.txt were there
    CHECK(read_all(vfs, "/docs/a.txt") == "edited by the user"); // a file is never truncated or rewritten
    CHECK(read_all(vfs, "/empty.txt") == "not empty any more");
    CHECK(read_all(vfs, "/docs/user.txt") == "my own data");     // a directory keeps its children
    CHECK(read_all(vfs, "/docs/sub/b.txt") == "bravo");
    std::printf("test_existing_entries_are_not_overwritten passed\n");
}

void test_repeated_seeding_is_safe() {
    MemoryFileSystem vfs;
    CHECK(seed_filesystem(vfs, kEntries).created == 5);
    write_all(vfs, "/docs/a.txt", "changed after the first run");
    for (int i = 0; i < 3; ++i) {
        const SeedResult again = seed_filesystem(vfs, kEntries);
        CHECK(again.ok() && again.created == 0 && again.skipped == 5);
    }
    CHECK(read_all(vfs, "/docs/a.txt") == "changed after the first run");
    CHECK((sorted_list(vfs, "/docs") == std::vector<std::string>{"a.txt", "sub"}));
    std::printf("test_repeated_seeding_is_safe passed\n");
}

void test_kind_conflicts_are_failures_and_change_nothing() {
    MemoryFileSystem vfs;
    write_all(vfs, "/docs", "a file where a directory is wanted");
    CHECK(vfs.create_directory("/empty.txt")); // a directory where a file is wanted
    const SeedResult r = seed_filesystem(vfs, kEntries);
    CHECK(!r.ok());
    // /docs conflicts; everything below it fails for want of a parent directory; /empty.txt conflicts.
    CHECK(r.failures.size() == 5);
    CHECK(r.created == 0);
    CHECK(r.failures[0].path == "/docs" && r.failures[0].reason.find("already exists as a file") != std::string::npos);
    CHECK(r.failures[1].path == "/docs/sub" && r.failures[1].reason.find("not a directory") != std::string::npos);
    CHECK(r.failures[4].path == "/empty.txt" && r.failures[4].reason.find("already exists as a directory") != std::string::npos);
    CHECK(read_all(vfs, "/docs") == "a file where a directory is wanted");
    CHECK(vfs.stat("/empty.txt")->is_directory);
    std::printf("test_kind_conflicts_are_failures_and_change_nothing passed\n");
}

void test_missing_parent_and_bad_paths() {
    MemoryFileSystem vfs;
    const std::vector<SeedEntry> entries = {
        SeedEntry::file("/nowhere/a.txt", "x"), // parents are not created implicitly
        SeedEntry::file("relative.txt", "x"),
        SeedEntry::file("", "x"),
        SeedEntry::file("/fine.txt", "ok"), // later entries are still processed
    };
    const SeedResult r = seed_filesystem(vfs, entries);
    CHECK(r.failures.size() == 3 && r.created == 1);
    CHECK(r.failures[0].path == "/nowhere/a.txt" && r.failures[0].reason.find("parent") != std::string::npos);
    CHECK(r.failures[1].path == "relative.txt" && r.failures[1].reason.find("not absolute") != std::string::npos);
    CHECK(r.failures[2].reason.find("not absolute") != std::string::npos);
    CHECK(!vfs.exists("/nowhere") && read_all(vfs, "/fine.txt") == "ok");
    std::printf("test_missing_parent_and_bad_paths passed\n");
}

void test_creation_failures_are_reported_and_do_not_stop_the_run() {
    FaultyFileSystem vfs;
    vfs.refuse_directory = {"/docs/sub"};
    vfs.refuse_file = {"/empty.txt"};
    const SeedResult r = seed_filesystem(vfs, kEntries);
    CHECK(!r.ok());
    CHECK(r.created == 2); // /docs and /docs/a.txt
    CHECK(r.failures.size() == 3);
    CHECK(r.failures[0].path == "/docs/sub" && !r.failures[0].reason.empty());
    CHECK(r.failures[1].path == "/docs/sub/b.txt" && r.failures[1].reason.find("parent") != std::string::npos); // knock-on, explained
    CHECK(r.failures[2].path == "/empty.txt" && !r.failures[2].reason.empty());
    CHECK(read_all(vfs, "/docs/a.txt") == "alpha\n"); // what could be created was
    CHECK(!vfs.exists("/docs/sub") && !vfs.exists("/empty.txt"));
    std::printf("test_creation_failures_are_reported_and_do_not_stop_the_run passed\n");
}

void test_incomplete_write_is_reported_and_leaves_no_file() {
    FaultyFileSystem vfs;
    vfs.short_write = {"/docs/a.txt"}; // "alpha\n" is 6 bytes, only 3 are accepted
    const SeedResult r = seed_filesystem(vfs, kEntries);
    CHECK(r.failures.size() == 1 && r.created == 4);
    CHECK(r.failures[0].path == "/docs/a.txt");
    CHECK(r.failures[0].reason.find("3 of 6") != std::string::npos);
    CHECK(!vfs.exists("/docs/a.txt")); // no half-written file
    // With a healthy filesystem the next run completes it.
    vfs.short_write.clear();
    const SeedResult retry = seed_filesystem(vfs, kEntries);
    CHECK(retry.ok() && retry.created == 1 && read_all(vfs, "/docs/a.txt") == "alpha\n");
    std::printf("test_incomplete_write_is_reported_and_leaves_no_file passed\n");
}

} // namespace

int main() {
    std::printf("Running VFS seeding tests...\n");
    test_creates_directories_and_files_with_contents();
    test_nothing_to_seed();
    test_existing_entries_are_not_overwritten();
    test_repeated_seeding_is_safe();
    test_kind_conflicts_are_failures_and_change_nothing();
    test_missing_parent_and_bad_paths();
    test_creation_failures_are_reported_and_do_not_stop_the_run();
    test_incomplete_write_is_reported_and_leaves_no_file();
    std::printf("All VFS seeding tests passed.\n");
    return 0;
}
