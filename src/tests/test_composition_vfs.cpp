// CompositionRoot applies the selected startup profile to its filesystem right after creating it: a default launch
// (no options) gets the development profile, and an explicit profile gets exactly that one.
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "../bootstrap/composition_root.hpp"

#include "test_check.hpp"

using namespace vynexos;

namespace {

std::vector<std::string> root_listing(bootstrap::CompositionRoot& root) {
    const auto vfs = root.virtual_file_system();
    CHECK(vfs != nullptr);
    auto names = vfs->list_directory("/");
    std::sort(names.begin(), names.end());
    return names;
}

const std::vector<std::string> kDevelopmentRoot = {"docs", "music", "notes.md", "pictures", "readme.txt", "todo.txt"};

void test_default_launch_gets_the_development_profile() {
    bootstrap::CompositionRoot root; // what main() builds for a plain launch
    CHECK(root_listing(root) == kDevelopmentRoot);
    CHECK(root.virtual_file_system()->exists("/docs/getting-started.md"));
    std::printf("test_default_launch_gets_the_development_profile passed\n");
}

void test_explicit_profiles() {
    bootstrap::StartupOptions empty;
    empty.vfs_profile = bootstrap::VfsProfile::Empty;
    bootstrap::CompositionRoot with_empty(empty);
    CHECK(root_listing(with_empty).empty());

    bootstrap::StartupOptions development;
    development.vfs_profile = bootstrap::VfsProfile::Development;
    bootstrap::CompositionRoot with_development(development);
    CHECK(root_listing(with_development) == kDevelopmentRoot);
    CHECK(with_development.virtual_file_system()->exists("/docs/getting-started.md"));

    // Independent instances: seeding one filesystem does not touch another.
    CHECK(root_listing(with_empty).empty());
    std::printf("test_explicit_profiles passed\n");
}

} // namespace

int main() {
    std::printf("Running CompositionRoot VFS tests...\n");
    test_default_launch_gets_the_development_profile();
    test_explicit_profiles();
    std::printf("All CompositionRoot VFS tests passed.\n");
    return 0;
}
