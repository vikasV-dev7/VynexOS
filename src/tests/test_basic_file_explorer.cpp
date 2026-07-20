#include "vynexos/apps/application.hpp"
#include "vynexos/core/virtual_file_system.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/hal/input_driver.hpp"
#include "../apps/basic_file_explorer.hpp"
#include <cassert>
#include <iostream>
#include <map>

using namespace vynexos;

class MockVirtualFileSystem : public core::IVirtualFileSystem {
public:
    struct MockNode {
        bool is_directory;
        std::vector<std::string> children;
    };
    std::map<std::string, MockNode> nodes;
    
    std::unique_ptr<core::IFileHandle> open(const std::string&) override { return nullptr; }
    std::unique_ptr<core::IFileHandle> create(const std::string&) override { return nullptr; }
    bool remove(const std::string&) override { return false; }
    bool create_directory(const std::string&) override { return false; }
    
    std::vector<std::string> list_directory(const std::string& path) const override {
        if (nodes.contains(path) && nodes.at(path).is_directory) {
            return nodes.at(path).children;
        }
        return {};
    }
    
    std::optional<core::FileStat> stat(const std::string& path) const override {
        if (nodes.contains(path)) {
            core::FileStat s;
            s.is_directory = nodes.at(path).is_directory;
            s.size_bytes = 0;
            return s;
        }
        return std::nullopt;
    }
    
    bool exists(const std::string& path) const noexcept override {
        return nodes.contains(path);
    }
};

class MockWidgetToolkit : public desktop::IWidgetToolkit {
public:
    void draw_rect(std::shared_ptr<desktop::ISurface>, int32_t, int32_t, uint32_t, uint32_t, desktop::Color) override {}
    void draw_text(std::shared_ptr<desktop::ISurface>, int32_t, int32_t, std::string_view, desktop::Color) override {}
    void draw_button(std::shared_ptr<desktop::ISurface>, int32_t, int32_t, uint32_t, uint32_t, std::string_view) override {}
    void draw_panel(std::shared_ptr<desktop::ISurface>, int32_t, int32_t, uint32_t, uint32_t) override {}
};

void test_initialization() {
    auto vfs = std::make_shared<MockVirtualFileSystem>();
    vfs->nodes["/"] = {true, {"bin", "home"}};
    vfs->nodes["/bin"] = {true, {}};
    vfs->nodes["/home"] = {true, {}};
    
    auto toolkit = std::make_shared<MockWidgetToolkit>();
    apps::BasicFileExplorer explorer(vfs, toolkit);
    
    assert(explorer.initialize());
    assert(explorer.get_current_path().as_string() == "/");
    assert(explorer.get_entries().size() == 2);
    // Because dirs are sorted alphabetically:
    assert(explorer.get_entries()[0].display_name == "bin");
    assert(explorer.get_entries()[1].display_name == "home");
    std::cout << "test_initialization passed\n";
}

void test_navigation_into_directory() {
    auto vfs = std::make_shared<MockVirtualFileSystem>();
    vfs->nodes["/"] = {true, {"home", "file.txt"}};
    vfs->nodes["/home"] = {true, {"user"}};
    vfs->nodes["/home/user"] = {true, {}};
    vfs->nodes["/file.txt"] = {false, {}};
    
    auto toolkit = std::make_shared<MockWidgetToolkit>();
    apps::BasicFileExplorer explorer(vfs, toolkit);
    explorer.initialize();
    
    core::Event ev;
    ev.topic = "HAL_INPUT_MOUSE";
    ev.payload = hal::MouseEvent(20, 85, 1);
    
    // Simulate click on "home" (row 0 since dirs are sorted first)
    explorer.handle_event(ev);
    
    assert(explorer.get_selected_index().has_value());
    assert(explorer.get_selected_index().value() == 0);
    assert(explorer.get_current_path().as_string() == "/");
    
    // Simulate second click on "home" (activate)
    explorer.handle_event(ev);
    
    assert(explorer.get_current_path().as_string() == "/home");
    assert(explorer.get_entries().size() == 1);
    assert(explorer.get_entries()[0].display_name == "user");
    
    // Selection should be reset
    assert(!explorer.get_selected_index().has_value());
    std::cout << "test_navigation_into_directory passed\n";
}

void test_navigation_to_parent() {
    auto vfs = std::make_shared<MockVirtualFileSystem>();
    vfs->nodes["/"] = {true, {"home"}};
    vfs->nodes["/home"] = {true, {"user"}};
    vfs->nodes["/home/user"] = {true, {}};
    
    auto toolkit = std::make_shared<MockWidgetToolkit>();
    apps::BasicFileExplorer explorer(vfs, toolkit);
    explorer.initialize();
    
    core::Event ev;
    ev.topic = "HAL_INPUT_MOUSE";
    ev.payload = hal::MouseEvent(20, 85, 1);
    
    // Click home, activate home
    explorer.handle_event(ev);
    explorer.handle_event(ev);
    
    assert(explorer.get_current_path().as_string() == "/home");
    
    // Click "Up" button (x:10-50, y:40-70)
    ev.payload = hal::MouseEvent(20, 50, 1);
    explorer.handle_event(ev);
    
    assert(explorer.get_current_path().as_string() == "/");
    std::cout << "test_navigation_to_parent passed\n";
}

void test_select_file_no_navigate() {
    auto vfs = std::make_shared<MockVirtualFileSystem>();
    vfs->nodes["/"] = {true, {"file.txt"}};
    vfs->nodes["/file.txt"] = {false, {}};
    
    auto toolkit = std::make_shared<MockWidgetToolkit>();
    apps::BasicFileExplorer explorer(vfs, toolkit);
    explorer.initialize();
    
    core::Event ev;
    ev.topic = "HAL_INPUT_MOUSE";
    ev.payload = hal::MouseEvent(20, 85, 1);
    
    // First click selects
    explorer.handle_event(ev);
    
    assert(explorer.get_selected_index().has_value());
    assert(explorer.get_selected_index().value() == 0);
    assert(explorer.get_current_path().as_string() == "/");
    
    // Second click should NOT navigate because it's a file
    explorer.handle_event(ev);
    
    assert(explorer.get_current_path().as_string() == "/");
    assert(explorer.get_selected_index().value() == 0);
    std::cout << "test_select_file_no_navigate passed\n";
}

void test_invalid_navigation() {
    auto vfs = std::make_shared<MockVirtualFileSystem>();
    vfs->nodes["/"] = {true, {"ghost_dir"}};
    // ghost_dir is missing from nodes on purpose (stat fails)
    
    auto toolkit = std::make_shared<MockWidgetToolkit>();
    apps::BasicFileExplorer explorer(vfs, toolkit);
    explorer.initialize();
    
    // The initialize will try to stat "ghost_dir", which fails, so it won't be in the entries!
    assert(explorer.get_entries().size() == 0);
    std::cout << "test_invalid_navigation passed\n";
}


int main() {
    std::cout << "Running BasicFileExplorer Tests...\n";
    test_initialization();
    test_navigation_into_directory();
    test_navigation_to_parent();
    test_select_file_no_navigate();
    test_invalid_navigation();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
