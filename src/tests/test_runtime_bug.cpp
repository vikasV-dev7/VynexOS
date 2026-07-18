#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "../bootstrap/composition_root.hpp"
#include "../hal/mock_input_driver.hpp"

using namespace vynexos;
using namespace vynexos::bootstrap;

int main() {
    std::cout << "Starting Runtime Bug Repro...\n";
    CompositionRoot root;
    root.initialize();
    
    auto mock_input = std::dynamic_pointer_cast<hal::MockInputDriver>(root.m_input_driver);
    if (!mock_input) {
        std::cout << "Skipping test: Not using MockInputDriver\n";
        return 0;
    }
    
    std::thread clicker([&root, mock_input]() {
        // Wait 1 second to let desktop apps initialize and render
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Terminal is at 200, 200, size 600x400
        std::cout << "[Test] Injecting MOUSE DOWN event at (300, 300)...\n";
        mock_input->inject_mouse(300, 300, 1);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::cout << "[Test] Injecting MOUSE UP event at (300, 300)...\n";
        mock_input->inject_mouse(300, 300, 0);
        
        // Wait 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::cout << "[Test] Shutting down...\n";
        root.shutdown();
    });
    
    root.run();
    clicker.join();
    
    return 0;
}
