#include "../core/memory_file_handle.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>

using namespace vynexos::core::internal;

void test_read_write() {
    auto file = std::make_shared<FileNode>("test.txt");
    MemoryFileHandle handle(file);

    assert(handle.position() == 0);
    assert(handle.size() == 0);

    // Test writing
    std::vector<uint8_t> write_data = { 1, 2, 3, 4, 5 };
    assert(handle.write(std::span(write_data)) == 5);
    assert(handle.position() == 5);
    assert(handle.size() == 5);

    // Test seeking
    assert(handle.seek(2));
    assert(handle.position() == 2);

    // Test read
    std::vector<uint8_t> read_buffer(2);
    assert(handle.read(std::span(read_buffer)) == 2);
    assert(read_buffer[0] == 3);
    assert(read_buffer[1] == 4);
    assert(handle.position() == 4);

    // Test read to EOF
    std::vector<uint8_t> eof_buffer(10);
    assert(handle.read(std::span(eof_buffer)) == 1);
    assert(eof_buffer[0] == 5);
    assert(handle.position() == 5);

    // Test invalid seek
    assert(!handle.seek(100));
}

void test_unlink_lifetime() {
    auto root = std::make_shared<DirectoryNode>("/");
    auto file = std::make_shared<FileNode>("temp.bin");
    root->add_child(file);

    MemoryFileHandle handle(std::static_pointer_cast<FileNode>(root->get_child("temp.bin")));

    // Unlink file
    root->remove_child("temp.bin");
    assert(root->children.empty());

    // Write should still succeed
    std::vector<uint8_t> write_data = { 0xAA, 0xBB };
    assert(handle.write(std::span(write_data)) == 2);
    
    // Size should reflect
    assert(handle.size() == 2);
}

void test_concurrent_access() {
    auto file = std::make_shared<FileNode>("shared.log");
    
    // We instantiate two handles wrapping the same FileNode
    MemoryFileHandle h1(file);
    MemoryFileHandle h2(file);

    std::vector<uint8_t> w1 = { 'A', 'A', 'A' };
    assert(h1.write(std::span(w1)) == 3);

    // h2 doesn't know about h1's cursor
    assert(h2.position() == 0);
    assert(h2.size() == 3);

    std::vector<uint8_t> r2(3);
    assert(h2.read(std::span(r2)) == 3);
    assert(r2[0] == 'A' && r2[1] == 'A' && r2[2] == 'A');
}

int main() {
    std::cout << "Running MemoryFileHandle Tests..." << std::endl;
    test_read_write();
    test_unlink_lifetime();
    test_concurrent_access();
    std::cout << "All MemoryFileHandle Tests Passed." << std::endl;
    return 0;
}
