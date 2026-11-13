// CompositionRoot shutdown is safe in every order a program can reach: explicit shutdown, shutdown called twice, the
// destructor after an explicit shutdown, the destructor alone (no explicit shutdown), and shutdown of a root that was
// never initialized. Each runs in a fresh root; a crash, hang or double teardown fails the test (CTest timeout).
#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <memory>

#include "../bootstrap/composition_root.hpp"
#include "vynexos/core/task_scheduler.hpp"
#include "vynexos/core/virtual_file_system.hpp"

#include "test_check.hpp"

using namespace vynexos;
using namespace std::chrono_literals;

namespace {

bootstrap::StartupOptions empty_profile() {
    bootstrap::StartupOptions options;
    options.vfs_profile = bootstrap::VfsProfile::Empty;
    return options;
}

void test_shutdown_without_initialize_twice() {
    bootstrap::CompositionRoot root(empty_profile());
    root.shutdown();
    root.shutdown(); // the second call does nothing
    CHECK(root.virtual_file_system() == nullptr); // torn down by the first
    std::printf("test_shutdown_without_initialize_twice passed\n");
} // and the destructor after it does nothing either

void test_destructor_alone_tears_down() {
    { bootstrap::CompositionRoot root(empty_profile()); }
    std::printf("test_destructor_alone_tears_down passed\n");
}

void test_initialized_root_shutdown_twice_then_destructor() {
    bootstrap::CompositionRoot root(empty_profile());
    root.initialize(); // display, drivers, shell and the system applications
    root.shutdown();
    root.shutdown();
    std::printf("test_initialized_root_shutdown_twice_then_destructor passed\n");
}

void test_initialized_root_destructor_without_shutdown() {
    {
        bootstrap::CompositionRoot root(empty_profile());
        root.initialize();
    } // e.g. leaving main() early: the destructor runs the full shutdown sequence
    std::printf("test_initialized_root_destructor_without_shutdown passed\n");
}

void test_shutdown_drains_async_work_before_teardown() {
    // A task still queued when shutdown() starts must run while the components it can reach are alive. The task holds
    // only a weak reference to the filesystem (which the teardown releases) and records whether it was still alive.
    bootstrap::CompositionRoot root(empty_profile());
    const std::weak_ptr<core::IVirtualFileSystem> vfs = root.virtual_file_system();
    std::promise<void> parked, open;
    auto parked_future = parked.get_future();
    auto open_future = open.get_future().share();
    std::atomic<int> vfs_alive_when_task_ran{-1};
    const bool accepted = root.task_scheduler()->enqueue([&parked, open_future, vfs, &vfs_alive_when_task_ran](const core::ExecutionContext&) {
        parked.set_value();
        open_future.wait();
        vfs_alive_when_task_ran = vfs.expired() ? 0 : 1;
    });
    CHECK(accepted);
    parked_future.wait(); // the task is running and held

    auto shutting_down = std::async(std::launch::async, [&root] { root.shutdown(); });
    // shutdown() cannot complete while the task is pending (a correct implementation never returns here, so this
    // bounded wait cannot produce a false failure; it also gives a broken one time to tear down underneath the task).
    CHECK(shutting_down.wait_for(300ms) == std::future_status::timeout);
    open.set_value();
    shutting_down.wait();
    CHECK(vfs_alive_when_task_ran == 1); // the task ran before the teardown
    CHECK(vfs.expired());                // and the teardown then happened
    std::printf("test_shutdown_drains_async_work_before_teardown passed\n");
}

} // namespace

int main() {
    std::printf("Running CompositionRoot shutdown tests...\n");
    test_shutdown_without_initialize_twice();
    test_destructor_alone_tears_down();
    test_initialized_root_shutdown_twice_then_destructor();
    test_initialized_root_destructor_without_shutdown();
    test_shutdown_drains_async_work_before_teardown();
    std::printf("All CompositionRoot shutdown tests passed.\n");
    return 0;
}
