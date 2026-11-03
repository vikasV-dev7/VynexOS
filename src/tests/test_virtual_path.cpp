#include "vynexos/core/virtual_path.hpp"
#include "test_check.hpp"
#include <iostream>

using namespace vynexos::core;

void test_normal_paths() {
    VirtualPath p1("/");
    CHECK(p1.is_absolute());
    CHECK(p1.is_root());
    CHECK(p1.as_string() == "/");

    VirtualPath p2("/usr/bin");
    CHECK(p2.is_absolute());
    CHECK(!p2.is_root());
    CHECK(p2.as_string() == "/usr/bin");
    CHECK(p2.filename() == "bin");

    VirtualPath p3("home/user");
    CHECK(!p3.is_absolute());
    CHECK(p3.as_string() == "home/user");
}

void test_normalization() {
    VirtualPath p1("/usr//bin");
    CHECK(p1.as_string() == "/usr/bin");

    VirtualPath p2("/usr/./bin");
    CHECK(p2.as_string() == "/usr/bin");

    VirtualPath p3("/usr/bin/..");
    CHECK(p3.as_string() == "/usr");

    VirtualPath p4("./folder");
    CHECK(p4.as_string() == "folder");

    VirtualPath p5("../folder");
    CHECK(p5.as_string() == "../folder");

    VirtualPath p6("/../folder");
    CHECK(p6.as_string() == "/folder"); // Traversing above root stays at root
}

void test_edge_cases() {
    VirtualPath p1("");
    CHECK(p1.is_empty());
    CHECK(p1.as_string() == "");

    VirtualPath p2("///");
    CHECK(p2.as_string() == "/");
    CHECK(p2.is_root());

    VirtualPath p3("/a/");
    CHECK(p3.as_string() == "/a");

    VirtualPath p4("a/b/c");
    CHECK(p4.parent().as_string() == "a/b");

    VirtualPath p5("file.txt");
    CHECK(p5.extension() == ".txt");
    
    VirtualPath p6(".hidden");
    CHECK(p6.extension() == ""); // Not an extension
    
    VirtualPath p7("a.b.c");
    CHECK(p7.extension() == ".c");
}

void test_equality() {
    VirtualPath p1("/usr/bin");
    VirtualPath p2("/usr/./bin/");
    CHECK(p1 == p2);

    VirtualPath p3("/usr");
    CHECK(p1 != p3);
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
