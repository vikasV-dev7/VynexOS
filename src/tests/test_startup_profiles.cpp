// The startup layer for the initial filesystem: the two profiles (their exact contents), applying them through the VFS
// interface, and command-line selection (default, valid and invalid values). The program-level behaviour (invalid
// options end the process with a non-zero status) is checked by the StartupRejectsBadOptions CTest.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "../bootstrap/startup_options.hpp"
#include "../bootstrap/vfs_profiles.hpp"
#include "vynexos/core/memory_file_system.hpp"

#include "test_check.hpp"

using namespace vynexos;
using namespace vynexos::bootstrap;

namespace {

std::string read_all(core::IVirtualFileSystem& vfs, const std::string& path) {
    auto handle = vfs.open(path);
    CHECK(handle != nullptr);
    std::vector<std::uint8_t> bytes(handle->size());
    CHECK(handle->read(bytes) == bytes.size());
    return std::string(bytes.begin(), bytes.end());
}
std::vector<std::string> sorted_list(const core::IVirtualFileSystem& vfs, const std::string& path) {
    auto names = vfs.list_directory(path);
    std::sort(names.begin(), names.end());
    return names;
}
/// Every path in the tree with its kind and contents, in a stable order: two identical filesystems give identical dumps.
void dump(core::IVirtualFileSystem& vfs, const std::string& dir, std::vector<std::string>& out) {
    for (const auto& name : sorted_list(vfs, dir)) {
        const std::string path = (dir == "/" ? "" : dir) + "/" + name;
        if (vfs.stat(path)->is_directory) {
            out.push_back("D " + path);
            dump(vfs, path, out);
        } else {
            out.push_back("F " + path + " = " + read_all(vfs, path));
        }
    }
}
std::vector<std::string> dump(core::IVirtualFileSystem& vfs) {
    std::vector<std::string> out;
    dump(vfs, "/", out);
    return out;
}

// ---- profiles ---------------------------------------------------------------------------------------------------------

void test_development_profile_exact_contents() {
    core::MemoryFileSystem vfs;
    const auto r = apply_vfs_profile(vfs, VfsProfile::Development);
    CHECK(r.ok() && r.created == 7 && r.skipped == 0);

    // The six top-level entries, no others.
    CHECK((sorted_list(vfs, "/") == std::vector<std::string>{"docs", "music", "notes.md", "pictures", "readme.txt", "todo.txt"}));
    for (const char* dir : {"/docs", "/music", "/pictures"}) CHECK(vfs.stat(dir).has_value() && vfs.stat(dir)->is_directory);
    for (const char* file : {"/notes.md", "/readme.txt", "/todo.txt"}) CHECK(vfs.stat(file).has_value() && !vfs.stat(file)->is_directory);

    // The nested sample file in docs/, and nothing else in any directory.
    CHECK((sorted_list(vfs, "/docs") == std::vector<std::string>{"getting-started.md"}));
    CHECK(vfs.stat("/docs/getting-started.md").has_value() && !vfs.stat("/docs/getting-started.md")->is_directory);
    CHECK(vfs.list_directory("/music").empty() && vfs.list_directory("/pictures").empty());

    // The exact contents (GUI checks and users rely on them).
    CHECK(read_all(vfs, "/notes.md") ==
          "# Notes\n"
          "\n"
          "This is the VynexOS development filesystem.\n"
          "It lives in memory and is recreated on every start.\n");
    CHECK(read_all(vfs, "/readme.txt") ==
          "VynexOS development profile\n"
          "---------------------------\n"
          "The system seeds this filesystem at startup.\n"
          "Start with --vfs-profile=empty for an empty one.\n");
    CHECK(read_all(vfs, "/todo.txt") ==
          "- Open File Explorer from the launcher\n"
          "- Browse docs/\n"
          "- Try the Terminal\n");
    CHECK(read_all(vfs, "/docs/getting-started.md") ==
          "# Getting started\n"
          "\n"
          "Open the launcher with the Vynex button to start the Terminal, File Explorer or the Demo App.\n");
    CHECK(vfs.stat("/notes.md")->size_bytes > 0 && vfs.stat("/readme.txt")->size_bytes > 0 && vfs.stat("/todo.txt")->size_bytes > 0);
    std::printf("test_development_profile_exact_contents passed\n");
}

void test_profiles_are_deterministic() {
    core::MemoryFileSystem a, b;
    CHECK(apply_vfs_profile(a, VfsProfile::Development).ok());
    CHECK(apply_vfs_profile(b, VfsProfile::Development).ok());
    CHECK(dump(a) == dump(b));
    CHECK(dump(a).size() == 7);
    const auto first = vfs_profile_entries(VfsProfile::Development);
    const auto second = vfs_profile_entries(VfsProfile::Development);
    CHECK(first.size() == 7 && first.data() == second.data()); // one fixed list, not rebuilt per call
    // Directories are listed before what they contain.
    CHECK(first[0].path == "/docs" && first[0].is_directory && first.back().path == "/docs/getting-started.md");
    std::printf("test_profiles_are_deterministic passed\n");
}

void test_empty_profile_creates_nothing() {
    core::MemoryFileSystem vfs;
    CHECK(vfs_profile_entries(VfsProfile::Empty).empty());
    const auto r = apply_vfs_profile(vfs, VfsProfile::Empty);
    CHECK(r.ok() && r.created == 0 && r.skipped == 0);
    CHECK(vfs.list_directory("/").empty());
    std::printf("test_empty_profile_creates_nothing passed\n");
}

void test_profile_leaves_existing_data_alone_and_is_repeatable() {
    core::MemoryFileSystem vfs;
    {
        auto notes = vfs.create("/notes.md");
        CHECK(notes != nullptr);
        const std::string mine = "my notes";
        const std::size_t written = notes->write(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(mine.data()), mine.size()));
        CHECK(written == mine.size());
    }
    CHECK(vfs.create_directory("/music"));
    CHECK(vfs.create("/music/song.txt") != nullptr);

    const auto first = apply_vfs_profile(vfs, VfsProfile::Development);
    CHECK(first.ok() && first.created == 5 && first.skipped == 2); // notes.md and music/ were already there
    CHECK(read_all(vfs, "/notes.md") == "my notes");
    CHECK((sorted_list(vfs, "/music") == std::vector<std::string>{"song.txt"}));

    const auto before = dump(vfs);
    for (int i = 0; i < 3; ++i) {
        const auto again = apply_vfs_profile(vfs, VfsProfile::Development);
        CHECK(again.ok() && again.created == 0 && again.skipped == 7);
    }
    CHECK(dump(vfs) == before);
    std::printf("test_profile_leaves_existing_data_alone_and_is_repeatable passed\n");
}

void test_profile_names() {
    CHECK(to_string(VfsProfile::Empty) == "empty" && to_string(VfsProfile::Development) == "development");
    CHECK(parse_vfs_profile("empty") == std::optional<VfsProfile>{VfsProfile::Empty});
    CHECK(parse_vfs_profile("development") == std::optional<VfsProfile>{VfsProfile::Development});
    for (const char* bad : {"", "Empty", "DEVELOPMENT", "dev", "development ", " empty", "production"}) CHECK(!parse_vfs_profile(bad).has_value());
    std::printf("test_profile_names passed\n");
}

// ---- command line -----------------------------------------------------------------------------------------------------

StartupParse parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> v(args);
    return parse_startup_options(v);
}

void test_default_profile_selection() {
    CHECK(StartupOptions{}.vfs_profile == VfsProfile::Development); // what a plain launch uses
    const auto none = parse({});
    CHECK(none.ok() && none.error.empty() && none.options.vfs_profile == VfsProfile::Development);
    const char* argv_plain[] = {"vynex_init"}; // main()'s form: argv[0] only
    const auto plain = parse_startup_options(1, argv_plain);
    CHECK(plain.ok() && plain.options.vfs_profile == VfsProfile::Development);
    std::printf("test_default_profile_selection passed\n");
}

void test_command_line_profiles() {
    CHECK(parse({"--vfs-profile=empty"}).ok() && parse({"--vfs-profile=empty"}).options.vfs_profile == VfsProfile::Empty);
    CHECK(parse({"--vfs-profile=development"}).ok() && parse({"--vfs-profile=development"}).options.vfs_profile == VfsProfile::Development);
    // A repeated option takes its last value.
    CHECK(parse({"--vfs-profile=empty", "--vfs-profile=development"}).options.vfs_profile == VfsProfile::Development);
    CHECK(parse({"--vfs-profile=development", "--vfs-profile=empty"}).options.vfs_profile == VfsProfile::Empty);
    // From main()'s argc/argv: argv[0] is the program, not an option.
    const char* argv[] = {"C:\\path\\vynex_init.exe", "--vfs-profile=empty"};
    const auto from_argv = parse_startup_options(2, argv);
    CHECK(from_argv.ok() && from_argv.options.vfs_profile == VfsProfile::Empty);
    std::printf("test_command_line_profiles passed\n");
}

void test_invalid_command_lines_are_rejected() {
    const auto bogus = parse({"--vfs-profile=bogus"});
    CHECK(!bogus.ok());
    CHECK(bogus.error.find("'bogus'") != std::string::npos && bogus.error.find("empty") != std::string::npos &&
          bogus.error.find("development") != std::string::npos); // names the value and the valid ones
    CHECK(!parse({"--vfs-profile=Empty"}).ok());               // names are exact
    CHECK(!parse({"--vfs-profile=empty "}).ok());
    CHECK(!parse({"--vfs-profile"}).ok());                     // no value
    CHECK(!parse({"--vfs-profile="}).ok());
    CHECK(!parse({"--vfs-profilex=empty"}).ok());              // a different option
    CHECK(!parse({"--nope"}).ok() && parse({"--nope"}).error.find("'--nope'") != std::string::npos);
    CHECK(!parse({"empty"}).ok());
    // One bad argument rejects the whole command line, wherever it is.
    CHECK(!parse({"--vfs-profile=empty", "--nope"}).ok());
    CHECK(!parse({"--vfs-profile=bogus", "--vfs-profile=empty"}).ok());
    const char* argv[] = {"vynex_init", "--vfs-profile=bogus"};
    CHECK(!parse_startup_options(2, argv).ok());
    CHECK(!startup_usage().empty() && startup_usage().find("--vfs-profile=empty|development") != std::string::npos);
    std::printf("test_invalid_command_lines_are_rejected passed\n");
}

} // namespace

int main() {
    std::printf("Running startup profile tests...\n");
    test_development_profile_exact_contents();
    test_profiles_are_deterministic();
    test_empty_profile_creates_nothing();
    test_profile_leaves_existing_data_alone_and_is_repeatable();
    test_profile_names();
    test_default_profile_selection();
    test_command_line_profiles();
    test_invalid_command_lines_are_rejected();
    std::printf("All startup profile tests passed.\n");
    return 0;
}
