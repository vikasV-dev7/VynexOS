#include "../core/vnode.hpp"
#include <cassert>
#include <iostream>

using namespace vynexos::core::internal;

void test_node_construction() {
    auto file = std::make_shared<FileNode>("test.txt");
    assert(file->get_name() == "test.txt");
    assert(file->get_type() == VNodeType::File);
    assert(file->data.empty());
    assert(file->get_size() == 0);

    auto dir = std::make_shared<DirectoryNode>("usr");
    assert(dir->get_name() == "usr");
    assert(dir->get_type() == VNodeType::Directory);
    assert(dir->children.empty());
}

void test_parent_ownership_and_insertion() {
    auto root = std::make_shared<DirectoryNode>("/");
    auto child_dir = std::make_shared<DirectoryNode>("home");
    auto child_file = std::make_shared<FileNode>("config.ini");

    // Add children
    root->add_child(child_dir);
    root->add_child(child_file);

    // Verify they exist in the map
    assert(root->children.size() == 2);
    assert(root->get_child("home") == child_dir);
    assert(root->get_child("config.ini") == child_file);

    // Verify parent links (weak_ptr behavior)
    assert(!child_dir->get_parent().expired());
    assert(child_dir->get_parent().lock() == root);
    
    assert(!child_file->get_parent().expired());
    assert(child_file->get_parent().lock() == root);
}

void test_child_removal_and_lifetime() {
    auto root = std::make_shared<DirectoryNode>("/");
    auto file = std::make_shared<FileNode>("temp.bin");
    
    root->add_child(file);
    assert(root->children.size() == 1);

    // Simulate an open file handle keeping the file alive
    std::shared_ptr<FileNode> open_handle_ref = std::static_pointer_cast<FileNode>(root->get_child("temp.bin"));

    // Remove from directory
    root->remove_child("temp.bin");
    assert(root->children.empty());
    assert(root->get_child("temp.bin") == nullptr);

    // The file should still be alive because the handle holds a reference
    assert(open_handle_ref != nullptr);
    assert(open_handle_ref->get_name() == "temp.bin");

    // The parent link should still be valid as long as root is alive
    assert(open_handle_ref->get_parent().lock() == root);

    // If root dies, the weak_ptr should expire
    root.reset();
    assert(open_handle_ref->get_parent().expired());
}

int main() {
    std::cout << "Running VNode Tests..." << std::endl;
    test_node_construction();
    test_parent_ownership_and_insertion();
    test_child_removal_and_lifetime();
    std::cout << "All VNode Tests Passed." << std::endl;
    return 0;
}
