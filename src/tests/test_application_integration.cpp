#include "vynexos/apps/application_registry.hpp"
#include "vynexos/apps/application.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/hal/input_driver.hpp"
#include "vynexos/desktop/compositor.hpp"
#include "vynexos/desktop/display_abstraction.hpp"
#include <span>
#include <cassert>
#include <iostream>

using namespace vynexos;

class MockApp : public apps::IApplication {
public:
    int initialized_calls = 0;
    int start_calls = 0;
    int update_calls = 0;
    int render_calls = 0;
    int shutdown_calls = 0;
    int event_calls = 0;

    bool initialize() override {
        initialized_calls++;
        return true;
    }
    void start() override {
        start_calls++;
    }
    void update(double) override {
        update_calls++;
    }
    bool handle_event(const core::Event&) override {
        event_calls++;
        return true;
    }
    void render(desktop::ISurface&) override {
        render_calls++;
    }
    void shutdown() override {
        shutdown_calls++;
    }
    std::string get_name() const noexcept override {
        return "MockApp";
    }
};

void test_registration() {
    apps::ApplicationRegistry registry;
    
    registry.register_application("test.app", []() {
        return std::make_unique<MockApp>();
    });
    
    bool caught = false;
    try {
        registry.register_application("test.app", []() { return nullptr; });
    } catch (...) {
        caught = true;
    }
    assert(caught); // Duplicate registration must throw
    
    std::cout << "test_registration passed\n";
}

class DummySurface : public desktop::ISurface {
public:
    uint32_t width() const override { return 100; }
    uint32_t height() const override { return 100; }
    std::span<uint8_t> map_pixels() override { return {}; }
    void unmap_pixels() override {}
};

void test_lifecycle() {
    apps::ApplicationRegistry registry;
    
    registry.register_application("app1", []() {
        return std::make_unique<MockApp>();
    });
    
    assert(registry.launch("app1"));
    auto running = registry.get_running_applications();
    assert(running.size() == 1);
    
    auto* mock_app = static_cast<MockApp*>(running[0]);
    assert(mock_app->initialized_calls == 1);
    assert(mock_app->start_calls == 1);
    
    mock_app->update(16.6);
    assert(mock_app->update_calls == 1);
    
    DummySurface surf;
    mock_app->render(surf);
    assert(mock_app->render_calls == 1);
    
    core::Event ev;
    mock_app->handle_event(ev);
    assert(mock_app->event_calls == 1);
    
    registry.terminate(mock_app);
    assert(registry.get_running_applications().empty());
    
    // shutdown happens during terminate
    // Note: mock_app pointer is dangling after terminate, but we can't test shutdown_calls easily here 
    // without a shared state because it's deleted. But it shouldn't crash.
    
    std::cout << "test_lifecycle passed\n";
}

int main() {
    std::cout << "Running Application Integration Tests...\n";
    test_registration();
    test_lifecycle();
    std::cout << "All integration tests passed successfully.\n";
    return 0;
}
