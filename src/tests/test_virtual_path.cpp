#include "vynexos/core/virtual_path.hpp"
#include <cassert>
#include <iostream>

using namespace vynexos::core;

void test_normal_paths() {
    VirtualPath p1("/");
    assert(p1.is_absolute());
    assert(p1.is_root());
    assert(p1.as_string() == "/");

    VirtualPath p2("/usr/bin");
    assert(p2.is_absolute());
    assert(!p2.is_root());
    assert(p2.as_string() == "/usr/bin");
    assert(p2.filename() == "bin");

    VirtualPath p3("home/user");
    assert(!p3.is_absolute());
    assert(p3.as_string() == "home/user");
}

void test_normalization() {
    VirtualPath p1("/usr//bin");
    assert(p1.as_string() == "/usr/bin");

    VirtualPath p2("/usr/./bin");
    assert(p2.as_string() == "/usr/bin");

    VirtualPath p3("/usr/bin/..");
    assert(p3.as_string() == "/usr");

    VirtualPath p4("./folder");
    assert(p4.as_string() == "folder");

    VirtualPath p5("../folder");
    assert(p5.as_string() == "../folder");

    VirtualPath p6("/../folder");
    assert(p6.as_string() == "/folder"); // Traversing above root stays at root
}

void test_edge_cases() {
    VirtualPath p1("");
    assert(p1.is_empty());
    assert(p1.as_string() == "");

    VirtualPath p2("///");
    assert(p2.as_string() == "/");
    assert(p2.is_root());

    VirtualPath p3("/a/");
    assert(p3.as_string() == "/a");

    VirtualPath p4("a/b/c");
    assert(p4.parent().as_string() == "a/b");

    VirtualPath p5("file.txt");
    assert(p5.extension() == ".txt");
    
    VirtualPath p6(".hidden");
    assert(p6.extension() == ""); // Not an extension
    
    VirtualPath p7("a.b.c");
    assert(p7.extension() == ".c");
}

void test_equality() {
    VirtualPath p1("/usr/bin");
    VirtualPath p2("/usr/./bin/");
    assert(p1 == p2);

    VirtualPath p3("/usr");
    assert(p1 != p3);
}

int main() {
    std::cout << "Running VirtualPath Tests..." << std::endl;
    test_normal_paths();
    test_normalization();
    test_edge_cases();
    test_equality();
    std::cout << "All VirtualPath Tests Passed." << std::endl;
    return 0;
}
