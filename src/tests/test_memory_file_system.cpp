#include <vynexos/core/memory_file_system.hpp>
#include <cassert>
#include <iostream>
#include <vector>

using namespace vynexos::core;

void test_basic_creation() {
    MemoryFileSystem vfs;
    
    // Root exists
    assert(vfs.exists("/"));
    auto stat = vfs.stat("/");
    assert(stat.has_value() && stat->is_directory);
    
    // Create directory
    assert(vfs.create_directory("/usr"));
    assert(vfs.create_directory("/usr/bin"));
    assert(vfs.exists("/usr/bin"));
    
    // Can't create directory if parent missing
    assert(!vfs.create_directory("/missing/dir"));
}

void test_file_operations() {
    MemoryFileSystem vfs;
    vfs.create_directory("/home");
    
    // Create file
    auto handle = vfs.create("/home/user.txt");
    assert(handle != nullptr);
    
    std::vector<uint8_t> data = { 'H', 'e', 'l', 'l', 'o' };
    assert(handle->write(std::span(data)) == 5);
    handle.reset(); // Close file
    
    // Stat file
    auto stat = vfs.stat("/home/user.txt");
    assert(stat.has_value());
    assert(!stat->is_directory);
    assert(stat->size_bytes == 5);
    
    // Open existing
    auto read_handle = vfs.open("/home/user.txt");
    assert(read_handle != nullptr);
    assert(read_handle->size() == 5);
    read_handle.reset();
    
    // Remove file
    assert(vfs.remove("/home/user.txt"));
    assert(!vfs.exists("/home/user.txt"));
}

void test_directory_listing() {
    MemoryFileSystem vfs;
    vfs.create_directory("/etc");
    vfs.create_directory("/etc/config");
    auto h1 = vfs.create("/etc/hosts");
    auto h2 = vfs.create("/etc/passwd");
    
    auto listing = vfs.list_directory("/etc");
    assert(listing.size() == 3);
    
    bool has_config = false, has_hosts = false, has_passwd = false;
    for (const auto& item : listing) {
        if (item == "config") has_config = true;
        if (item == "hosts") has_hosts = true;
        if (item == "passwd") has_passwd = true;
    }
    assert(has_config && has_hosts && has_passwd);
    
    // Cannot remove non-empty directory
    assert(!vfs.remove("/etc"));
    
    // Can remove empty directory
    assert(vfs.remove("/etc/config"));
}

void test_path_normalization_usage() {
    MemoryFileSystem vfs;
    vfs.create_directory("/usr");
    auto h = vfs.create("/usr/test.bin");
    h.reset();
    
    // Path normalization should work properly through VirtualPath integration
    assert(vfs.exists("/usr/../usr/./test.bin"));
    assert(vfs.remove("/usr/../usr/./test.bin"));
    assert(!vfs.exists("/usr/test.bin"));
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
