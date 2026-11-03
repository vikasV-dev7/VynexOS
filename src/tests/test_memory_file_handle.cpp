#include "../core/memory_file_handle.hpp"
#include "test_check.hpp"
#include <iostream>
#include <vector>
#include <thread>

using namespace vynexos::core::internal;

void test_read_write() {
    auto file = std::make_shared<FileNode>("test.txt");
    MemoryFileHandle handle(file);

    CHECK(handle.position() == 0);
    CHECK(handle.size() == 0);

    // Test writing
    std::vector<uint8_t> write_data = { 1, 2, 3, 4, 5 };
    const std::size_t written = handle.write(std::span(write_data));
    CHECK(written == 5);
    CHECK(handle.position() == 5);
    CHECK(handle.size() == 5);

    // Test seeking
    const bool sought = handle.seek(2);
    CHECK(sought);
    CHECK(handle.position() == 2);

    // Test read
    std::vector<uint8_t> read_buffer(2);
    const std::size_t got = handle.read(std::span(read_buffer));
    CHECK(got == 2);
    CHECK(read_buffer[0] == 3);
    CHECK(read_buffer[1] == 4);
    CHECK(handle.position() == 4);

    // Test read to EOF
    std::vector<uint8_t> eof_buffer(10);
    const std::size_t got_eof = handle.read(std::span(eof_buffer));
    CHECK(got_eof == 1);
    CHECK(eof_buffer[0] == 5);
    CHECK(handle.position() == 5);

    // Test invalid seek
    const bool sought_far = handle.seek(100);
    CHECK(!sought_far);
}

void test_unlink_lifetime() {
    auto root = std::make_shared<DirectoryNode>("/");
    auto file = std::make_shared<FileNode>("temp.bin");
    root->add_child(file);

    MemoryFileHandle handle(std::static_pointer_cast<FileNode>(root->get_child("temp.bin")));

    // Unlink file
    root->remove_child("temp.bin");
    CHECK(root->children.empty());

    // Write should still succeed
    std::vector<uint8_t> write_data = { 0xAA, 0xBB };
    const std::size_t written = handle.write(std::span(write_data));
    CHECK(written == 2);
    
    // Size should reflect
    CHECK(handle.size() == 2);
}

void test_concurrent_access() {
    auto file = std::make_shared<FileNode>("shared.log");
    
    // We instantiate two handles wrapping the same FileNode
    MemoryFileHandle h1(file);
    MemoryFileHandle h2(file);

    std::vector<uint8_t> w1 = { 'A', 'A', 'A' };
    const std::size_t written1 = h1.write(std::span(w1));
    CHECK(written1 == 3);

    // h2 doesn't know about h1's cursor
    CHECK(h2.position() == 0);
    CHECK(h2.size() == 3);

    std::vector<uint8_t> r2(3);
    const std::size_t read2 = h2.read(std::span(r2));
    CHECK(read2 == 3);
    CHECK(r2[0] == 'A' && r2[1] == 'A' && r2[2] == 'A');
}

int main() {
    std::cout << "Running MemoryFileHandle Tests..." << std::endl;
    test_read_write();
    test_unlink_lifetime();
    test_concurrent_access();
    std::cout << "All MemoryFileHandle Tests Passed." << std::endl;
    return 0;
}
