#include <vynexos/apps/application_registry.hpp>
#include <vynexos/apps/application.hpp>
#include <cassert>
#include <iostream>
#include <stdexcept>

// Minimal Mock Event (if not included)
namespace vynexos::core { struct Event {}; }
namespace vynexos::desktop { class ISurface {}; }

using namespace vynexos::apps;

class MockApplication : public IApplication {
public:
    MockApplication(std::string name, bool init_success) 
        : m_name(std::move(name)), m_init_success(init_success) {}
        
    ~MockApplication() override {
        // We assert that shutdown must be called. However, if init failed, 
        // shutdown shouldn't be called.
        if (m_init_called && m_init_success) {
            assert(m_shutdown_called && "Shutdown should be called before destruction if started");
        }
    }

    bool initialize() override {
        m_init_called = true;
        return m_init_success;
    }

    void start() override {
        m_start_called = true;
    }

    void update(double /*delta_time_ms*/) override {}
    bool handle_event(const vynexos::core::Event& /*event*/) override { return false; }
    void render(vynexos::desktop::ISurface& /*target_surface*/) override {}

    void shutdown() override {
        m_shutdown_called = true;
    }

    std::string get_name() const noexcept override {
        return m_name;
    }

    bool m_init_called{false};
    bool m_start_called{false};
    bool m_shutdown_called{false};
private:
    std::string m_name;
    bool m_init_success;
};

void test_registration_and_launch() {
    ApplicationRegistry registry;
    
    registry.register_application("mock.app", []() {
        return std::make_unique<MockApplication>("Mock", true);
    });

    // Duplicate registration should throw
    bool threw = false;
    try {
        registry.register_application("mock.app", nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    // Launch non-existent
    assert(!registry.launch("non.existent"));

    // Launch valid
    assert(registry.launch("mock.app"));
    
    auto running = registry.get_running_applications();
    assert(running.size() == 1);
    
    auto mock_app = static_cast<MockApplication*>(running[0]);
    assert(mock_app->m_init_called);
    assert(mock_app->m_start_called);
    assert(!mock_app->m_shutdown_called);
}

void test_failed_initialization() {
    ApplicationRegistry registry;
    
    registry.register_application("fail.app", []() {
        return std::make_unique<MockApplication>("Fail", false);
    });

    assert(!registry.launch("fail.app"));
    assert(registry.get_running_applications().empty());
}

void test_termination() {
    ApplicationRegistry registry;
    
    registry.register_application("mock.app", []() {
        return std::make_unique<MockApplication>("Mock", true);
    });

    registry.launch("mock.app");
    registry.launch("mock.app");
    
    auto running = registry.get_running_applications();
    assert(running.size() == 2);
    
    registry.terminate(running[0]);
    
    auto remaining = registry.get_running_applications();
    assert(remaining.size() == 1);
    assert(remaining[0] == running[1]);
    
    registry.terminate_all();
    assert(registry.get_running_applications().empty());
}

int main() {
    std::cout << "Running ApplicationRegistry Tests..." << std::endl;
    test_registration_and_launch();
    test_failed_initialization();
    test_termination();
    std::cout << "All ApplicationRegistry Tests Passed." << std::endl;
    return 0;
}
