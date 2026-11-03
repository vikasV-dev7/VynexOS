#include "../core/vnode.hpp"
#include "test_check.hpp"
#include <iostream>

using namespace vynexos::core::internal;

void test_node_construction() {
    auto file = std::make_shared<FileNode>("test.txt");
    CHECK(file->get_name() == "test.txt");
    CHECK(file->get_type() == VNodeType::File);
    CHECK(file->data.empty());
    CHECK(file->get_size() == 0);

    auto dir = std::make_shared<DirectoryNode>("usr");
    CHECK(dir->get_name() == "usr");
    CHECK(dir->get_type() == VNodeType::Directory);
    CHECK(dir->children.empty());
}

void test_parent_ownership_and_insertion() {
    auto root = std::make_shared<DirectoryNode>("/");
    auto child_dir = std::make_shared<DirectoryNode>("home");
    auto child_file = std::make_shared<FileNode>("config.ini");

    // Add children
    root->add_child(child_dir);
    root->add_child(child_file);

    // Verify they exist in the map
    CHECK(root->children.size() == 2);
    CHECK(root->get_child("home") == child_dir);
    CHECK(root->get_child("config.ini") == child_file);

    // Verify parent links (weak_ptr behavior)
    CHECK(!child_dir->get_parent().expired());
    CHECK(child_dir->get_parent().lock() == root);
    
    CHECK(!child_file->get_parent().expired());
    CHECK(child_file->get_parent().lock() == root);
}

void test_child_removal_and_lifetime() {
    auto root = std::make_shared<DirectoryNode>("/");
    auto file = std::make_shared<FileNode>("temp.bin");
    
    root->add_child(file);
    CHECK(root->children.size() == 1);

    // Simulate an open file handle keeping the file alive
    std::shared_ptr<FileNode> open_handle_ref = std::static_pointer_cast<FileNode>(root->get_child("temp.bin"));

    // Remove from directory
    root->remove_child("temp.bin");
    CHECK(root->children.empty());
    CHECK(root->get_child("temp.bin") == nullptr);

    // The file should still be alive because the handle holds a reference
    CHECK(open_handle_ref != nullptr);
    CHECK(open_handle_ref->get_name() == "temp.bin");

    // The parent link should still be valid as long as root is alive
    CHECK(open_handle_ref->get_parent().lock() == root);

    // If root dies, the weak_ptr should expire
    root.reset();
    CHECK(open_handle_ref->get_parent().expired());
}

int main() {
    std::cout << "Running VNode Tests..." << std::endl;
    test_node_construction();
    test_parent_ownership_and_insertion();
    test_child_removal_and_lifetime();
    std::cout << "All VNode Tests Passed." << std::endl;
    return 0;
}
