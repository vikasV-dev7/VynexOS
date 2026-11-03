#include <vynexos/core/memory_file_system.hpp>
#include "test_check.hpp"
#include <iostream>
#include <vector>

using namespace vynexos::core;

void test_basic_creation() {
    MemoryFileSystem vfs;
    
    // Root exists
    CHECK(vfs.exists("/"));
    auto stat = vfs.stat("/");
    CHECK(stat.has_value() && stat->is_directory);
    
    // Create directory
    const bool created_usr = vfs.create_directory("/usr");
    CHECK(created_usr);
    const bool created_bin = vfs.create_directory("/usr/bin");
    CHECK(created_bin);
    CHECK(vfs.exists("/usr/bin"));
    
    // Can't create directory if parent missing
    const bool created_orphan = vfs.create_directory("/missing/dir");
    CHECK(!created_orphan);
}

void test_file_operations() {
    MemoryFileSystem vfs;
    vfs.create_directory("/home");
    
    // Create file
    auto handle = vfs.create("/home/user.txt");
    CHECK(handle != nullptr);
    
    std::vector<uint8_t> data = { 'H', 'e', 'l', 'l', 'o' };
    const std::size_t written = handle->write(std::span(data));
    CHECK(written == 5);
    handle.reset(); // Close file
    
    // Stat file
    auto stat = vfs.stat("/home/user.txt");
    CHECK(stat.has_value());
    CHECK(!stat->is_directory);
    CHECK(stat->size_bytes == 5);
    
    // Open existing
    auto read_handle = vfs.open("/home/user.txt");
    CHECK(read_handle != nullptr);
    CHECK(read_handle->size() == 5);
    read_handle.reset();
    
    // Remove file
    const bool removed = vfs.remove("/home/user.txt");
    CHECK(removed);
    CHECK(!vfs.exists("/home/user.txt"));
}

void test_directory_listing() {
    MemoryFileSystem vfs;
    vfs.create_directory("/etc");
    vfs.create_directory("/etc/config");
    auto h1 = vfs.create("/etc/hosts");
    auto h2 = vfs.create("/etc/passwd");
    
    auto listing = vfs.list_directory("/etc");
    CHECK(listing.size() == 3);
    
    bool has_config = false, has_hosts = false, has_passwd = false;
    for (const auto& item : listing) {
        if (item == "config") has_config = true;
        if (item == "hosts") has_hosts = true;
        if (item == "passwd") has_passwd = true;
    }
    CHECK(has_config && has_hosts && has_passwd);
    
    // Cannot remove non-empty directory
    const bool removed_non_empty = vfs.remove("/etc");
    CHECK(!removed_non_empty);
    
    // Can remove empty directory
    const bool removed_empty_dir = vfs.remove("/etc/config");
    CHECK(removed_empty_dir);
}

void test_path_normalization_usage() {
    MemoryFileSystem vfs;
    vfs.create_directory("/usr");
    auto h = vfs.create("/usr/test.bin");
    h.reset();
    
    // Path normalization should work properly through VirtualPath integration
    CHECK(vfs.exists("/usr/../usr/./test.bin"));
    const bool removed_normalised = vfs.remove("/usr/../usr/./test.bin");
    CHECK(removed_normalised);
    CHECK(!vfs.exists("/usr/test.bin"));
}

int main() {
    std::cout << "Running MemoryFileSystem Tests..." << std::endl;
    test_basic_creation();
    test_file_operations();
    test_directory_listing();
    test_path_normalization_usage();
    std::cout << "All MemoryFileSystem Tests Passed." << std::endl;
    return 0;
}
