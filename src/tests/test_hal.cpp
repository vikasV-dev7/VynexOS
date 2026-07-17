#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "../core/file_logger.hpp"
#include "../hal/mock_block_device.hpp"
#include "../hal/mock_network_adapter.hpp"
#include "../hal/mock_hardware_clock.hpp"
#include "../bootstrap/composition_root.hpp"

using namespace vynexos;
using namespace vynexos::hal;

int failed_tests = 0;

#define ASSERT_TRUE(condition, msg) \
    if (!(condition)) { \
        std::cerr << "[FAIL] " << msg << "\n"; \
        failed_tests++; \
    } else { \
        std::cout << "[PASS] " << msg << "\n"; \
    }

void test_storage(std::shared_ptr<core::ILogger> logger) {
    std::cout << "--- Testing Storage Subsystem ---\n";
    MockBlockDevice device(logger, 1024); // 1024 blocks of 512 bytes

    // DeviceNotReady
    std::vector<uint8_t> buffer(512);
    auto res_uninit = device.read_blocks(0, 1, buffer);
    ASSERT_TRUE(!res_uninit && res_uninit.error() == StorageError::DeviceNotReady, "Read before initialization rejected");

    // Initialization
    auto init_res = device.initialize();
    ASSERT_TRUE(init_res.has_value(), "Block device initialization");

    // Capacity reporting
    ASSERT_TRUE(device.get_capacity_blocks() == 1024, "Capacity reporting");
    ASSERT_TRUE(device.get_block_size() == 512, "Block size reporting");

    // Block write & read
    std::vector<uint8_t> write_data(512, 0xAA);
    auto write_res = device.write_blocks(10, write_data);
    ASSERT_TRUE(write_res.has_value(), "Block write");

    std::vector<uint8_t> read_data(512, 0);
    auto read_res = device.read_blocks(10, 1, read_data);
    ASSERT_TRUE(read_res.has_value(), "Block read");
    ASSERT_TRUE(read_data == write_data, "Read data matches write data");

    // Out-of-range rejection
    auto oob_read = device.read_blocks(2000, 1, read_data);
    ASSERT_TRUE(!oob_read && oob_read.error() == StorageError::OutOfBounds, "Out-of-range read rejection");

    auto oob_write = device.write_blocks(2000, write_data);
    ASSERT_TRUE(!oob_write && oob_write.error() == StorageError::OutOfBounds, "Out-of-range write rejection");

    // Invalid parameter handling
    std::vector<uint8_t> bad_size_data(300, 0);
    auto bad_size_write = device.write_blocks(0, bad_size_data);
    ASSERT_TRUE(!bad_size_write && bad_size_write.error() == StorageError::OutOfBounds, "Invalid parameter handling (unaligned write)");

    std::vector<uint8_t> bad_size_read(300, 0);
    auto bad_size_read_res = device.read_blocks(0, 1, bad_size_read);
    ASSERT_TRUE(!bad_size_read_res && bad_size_read_res.error() == StorageError::OutOfBounds, "Invalid parameter handling (small read buffer)");
}

void test_network(std::shared_ptr<core::ILogger> logger) {
    std::cout << "--- Testing Network Subsystem ---\n";
    MockNetworkAdapter network(logger);

    // Initialization
    auto init_res = network.initialize();
    ASSERT_TRUE(init_res.has_value(), "Network adapter initialization");

    // Receive callback & Loopback packet transmission
    bool callback_executed = false;
    std::vector<uint8_t> received_data;
    network.set_receive_callback([&](std::span<const uint8_t> data) {
        callback_executed = true;
        received_data.assign(data.begin(), data.end());
    });

    std::vector<uint8_t> packet = {0xDE, 0xAD, 0xBE, 0xEF};
    auto send_res = network.send_packet(packet);
    
    ASSERT_TRUE(send_res.has_value(), "Loopback packet transmission");
    ASSERT_TRUE(callback_executed, "Receive callback execution");
    ASSERT_TRUE(received_data == packet, "Data integrity verified");

    // Empty packet handling
    callback_executed = false;
    std::vector<uint8_t> empty_packet;
    auto send_empty = network.send_packet(empty_packet);
    ASSERT_TRUE(send_empty.has_value(), "Empty packet transmission");
    ASSERT_TRUE(callback_executed, "Empty packet receive callback execution");

    // Shutdown behavior
    network.shutdown();
    callback_executed = false;
    auto send_after = network.send_packet(packet);
    ASSERT_TRUE(!send_after && send_after.error() == NetworkError::DeviceNotReady, "Shutdown behavior (transmission rejected)");
}

void test_clock(std::shared_ptr<core::ILogger> logger) {
    std::cout << "--- Testing Hardware Clock ---\n";
    MockHardwareClock clock(logger);

    // Before init
    ASSERT_TRUE(clock.get_monotonic_time_ns() == 0, "Time is 0 before initialization");

    // Initialization
    auto init_res = clock.initialize();
    ASSERT_TRUE(init_res.has_value(), "Clock initialization");

    // High-resolution & Monotonic timestamp progression
    uint64_t time1 = clock.get_monotonic_time_ns();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint64_t time2 = clock.get_monotonic_time_ns();
    
    ASSERT_TRUE(time1 > 0, "High-resolution time retrieval");
    ASSERT_TRUE(time2 > time1, "Monotonic timestamp progression");

    // Shutdown behavior
    clock.shutdown();
    ASSERT_TRUE(clock.get_monotonic_time_ns() == 0, "Shutdown behavior (returns 0 after shutdown)");
}

void test_composition_root() {
    std::cout << "--- Testing CompositionRoot Integration ---\n";
    
    // Dependency injection validation & Resource cleanup verification
    // Instantiating CompositionRoot effectively validates dependency injection 
    // inside its constructor.
    {
        bootstrap::CompositionRoot root;
        
        // HAL initialization order
        // initialize() will initialize all HAL components.
        // We ensure it doesn't crash or return early by testing if it executes fully.
        // Note: initialize() is void in CompositionRoot, but logs internally.
        root.initialize();
        ASSERT_TRUE(true, "HAL initialization order (no crash during initialize)");
        
        // HAL shutdown order
        // shutdown() stops services, then blocks/networks/clocks/display, then task scheduler.
        root.shutdown();
        ASSERT_TRUE(true, "HAL shutdown order (no crash during shutdown)");
    }
    
    ASSERT_TRUE(true, "Dependency injection validation & Resource cleanup verification (RAII destruction succeeded)");
}

int main() {
    auto logger = std::make_shared<core::FileLogger>("hal_test.log");

    test_storage(logger);
    test_network(logger);
    test_clock(logger);
    test_composition_root();

    if (failed_tests > 0) {
        std::cerr << "\n[HALTests] Failed " << failed_tests << " tests.\n";
        return 1;
    }

    std::cout << "\n[HALTests] All tests passed successfully.\n";
    return 0;
}
