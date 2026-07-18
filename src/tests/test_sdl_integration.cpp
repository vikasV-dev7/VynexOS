#include <iostream>
#include <cassert>
#include <chrono>
#include <cassert>
#include <thread>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "vynexos/core/frame_clock.hpp"
#include "../desktop/sdl2_display_backend.hpp"
#include "../hal/sdl2_input_driver.hpp"
#include "vynexos/core/event_bus.hpp"
#include "vynexos/hal/input_driver.hpp"
#include "vynexos/core/logger.hpp"

using namespace vynexos;

class DummyLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Info; }
    void log_raw(core::LogLevel, std::string_view) override {}
};

class DummyEventBus : public core::IEventBus {
public:
    size_t event_count = 0;
    std::string last_topic;

    void publish(std::shared_ptr<const core::Event> event) override {
        event_count++;
        last_topic = event->topic;
    }
    void subscribe(const std::string&, EventHandler) override {}
};

void test_frame_clock_stability() {
    std::cout << "Running test_frame_clock_stability...\n";
    core::FrameClock clock(60);
    
    auto start_time = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; i++) {
        clock.begin_frame();
        // simulate a tiny bit of work
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        clock.end_frame_and_wait();
    }
    auto end_time = std::chrono::steady_clock::now();
    
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    // std::this_thread::sleep_for is notoriously inaccurate on Windows (can oversleep by 15ms).
    // So we just verify it didn't run too FAST (which would mean the clock is failing to wait).
    if (elapsed_ms < 15000) {
        std::cout << "  -> FAILED: Frame clock ran too fast! (" << elapsed_ms << " ms)" << std::endl;
        std::exit(1);
    }
    std::cout << "  -> Passed (1000 frames took " << elapsed_ms << " ms)." << std::endl;
}

void test_sdl_raii_cycles() {
    std::cout << "Running test_sdl_raii_cycles...\n";
    auto logger = std::make_shared<DummyLogger>();
    
    // Perform multiple init/shutdown cycles to ensure no dangling resources
    for (int i = 0; i < 5; i++) {
        desktop::SDL2DisplayBackend backend(logger);
        auto res = backend.initialize();
        if (res) {
            SDL_PumpEvents(); // Prevent deadlock during window destruction on Windows
            backend.shutdown();
        }
    }
    std::cout << "  -> Passed (No crashes during RAII cycles)." << std::endl;
}

void test_burst_event_processing() {
    std::cout << "Running test_burst_event_processing...\n";
    auto logger = std::make_shared<DummyLogger>();
    auto bus = std::make_shared<DummyEventBus>();
    
    hal::SDL2InputDriver driver(bus, logger);
    
    // Inject synthetic SDL events directly into the queue
    SDL_Event ev1;
    ev1.type = SDL_KEYDOWN;
    ev1.key.keysym.sym = SDLK_ESCAPE;
    SDL_PushEvent(&ev1);
    
    SDL_Event ev2;
    ev2.type = SDL_MOUSEMOTION;
    ev2.motion.x = 100;
    ev2.motion.y = 200;
    ev2.motion.state = SDL_BUTTON_LMASK;
    SDL_PushEvent(&ev2);
    
    driver.poll(); // Drain the queue
    
    if (bus->event_count != 2) {
        std::cout << "  -> FAILED: Expected 2 events, got " << bus->event_count << std::endl;
        std::exit(1);
    }
    if (bus->last_topic != "HAL_INPUT_MOUSE") {
        std::cout << "  -> FAILED: Expected HAL_INPUT_MOUSE topic" << std::endl;
        std::exit(1);
    }
    std::cout << "  -> Passed (Burst event processing complete)." << std::endl;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "Starting SDLIntegrationTests..." << std::endl;
    SDL_SetMainReady(); // Prevent SDL from hijacking main
    
    test_frame_clock_stability();
    test_sdl_raii_cycles();
    test_burst_event_processing();
    
    std::cout << "All SDLIntegrationTests passed successfully." << std::endl;
    return 0;
}
