#include "vynexos/core/basic_task_scheduler.hpp"
#include "vynexos/core/logger.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>

using namespace vynexos::core;

class DummyLogger : public ILogger {
public:
    void log(LogLevel, const std::string&) override {}
    void debug(const std::string&) override {}
    void info(const std::string&) override {}
    void warning(const std::string&) override {}
    void error(const std::string&) override {}
    void fatal(const std::string&) override {}
};

void run_concurrency_tests() {
    DummyLogger logger;
    
    std::cout << "Starting Concurrency Tests...\n";
    
    // Test 1: Multiple Producers & Execution Guarantee
    {
        BasicTaskScheduler scheduler(logger, 4);
        std::atomic<int> counter{0};
        
        std::vector<std::thread> producers;
        for (int i = 0; i < 4; ++i) {
            producers.emplace_back([&scheduler, &counter]() {
                for (int j = 0; j < 10000; ++j) {
                    scheduler.enqueue([&counter](const ExecutionContext&) {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            });
        }
        
        for (auto& t : producers) t.join();
        
        scheduler.shutdown();
        
        assert(counter.load() == 40000 && "Execution Guarantee failed!");
        std::cout << "[PASS] Multiple Producers & Execution Guarantee\n";
    }

    // Test 2: Active Shutdown & Rejection Verification & Execution Match Guarantee
    {
        BasicTaskScheduler scheduler(logger, 4);
        std::atomic<int> success_enqueue_count{0};
        std::atomic<int> execute_count{0};
        std::atomic<int> rejected_count{0};
        
        std::vector<std::thread> producers;
        for (int i = 0; i < 4; ++i) {
            producers.emplace_back([&scheduler, &success_enqueue_count, &execute_count, &rejected_count]() {
                for (int j = 0; j < 10000; ++j) {
                    bool accepted = scheduler.enqueue([&execute_count](const ExecutionContext&) {
                        execute_count.fetch_add(1, std::memory_order_relaxed);
                    });
                    
                    if (accepted) {
                        success_enqueue_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        rejected_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        
        // Let them run briefly, then call shutdown while they are active
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        scheduler.shutdown(); // This will trigger rejection for remaining enqueue calls
        
        for (auto& t : producers) t.join();
        
        // Because shutdown() drains, execute_count MUST exactly equal success_enqueue_count.
        // And rejected_count MUST exactly equal (40000 - success_enqueue_count).
        int executed = execute_count.load();
        int accepted = success_enqueue_count.load();
        int rejected = rejected_count.load();
        
        assert(executed == accepted && "Active Shutdown Drain failed!");
        assert(rejected > 0 && "Rejection Verification failed (no tasks were rejected)!");
        assert(executed + rejected == 40000 && "Total tasks mismatch!");
        
        std::cout << "[PASS] Active Shutdown & Rejection Verification\n";
    }
    
    std::cout << "[PASS] Resource Cleanup (No hanging threads after scope exit)\n";
}

int main() {
    run_concurrency_tests();
    return 0;
}
