// Shutdown safety of asynchronous event delivery: the event bus, the task scheduler and a real subscriber (the desktop
// shell). Asynchronous deliveries are scheduler tasks that can wait in the queue while their subscriber, or the bus
// itself, is destroyed; none of them may then reach a destroyed object.
//
// Determinism: every test that needs events to stay queued uses a one-worker scheduler whose worker is parked inside a
// "gate" task until the test opens the gate. Draining is observed through scheduler.shutdown(), which returns only after
// every accepted task has run. No test relies on a sleep for correctness; the one bounded wait (in
// test_unsubscribe_waits_for_a_running_handler) can only fail for a broken implementation, never flake a correct one.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "../apps/desktop_shell.hpp"
#include "../core/basic_task_scheduler.hpp"
#include "../core/in_memory_event_bus.hpp"
#include "../desktop/basic_compositor.hpp"
#include "../desktop/basic_taskbar.hpp"
#include "../desktop/basic_widget_toolkit.hpp"
#include "../desktop/basic_window_decorator.hpp"
#include "../desktop/basic_window_manager.hpp"
#include "../desktop/mock_display_backend.hpp"
#include "vynexos/core/logger.hpp"
#include "vynexos/desktop/notification_service.hpp"
#include "vynexos/hal/input_driver.hpp"

#include "test_check.hpp"

using namespace vynexos;
using namespace std::chrono_literals;

namespace {

class CountingLogger : public core::ILogger {
public:
    void set_log_level(core::LogLevel) override {}
    [[nodiscard]] core::LogLevel get_log_level() const override { return core::LogLevel::Debug; }
    void log_raw(core::LogLevel level, std::string_view) override {
        if (level == core::LogLevel::Warning) ++warnings;
        if (level == core::LogLevel::Error || level == core::LogLevel::Fatal || level == core::LogLevel::Critical) ++errors;
    }
    std::atomic<int> warnings{0};
    std::atomic<int> errors{0};
};

/// Parks a scheduler worker until open() is called; entered() returns once the worker is parked.
class Gate {
public:
    void park(core::ITaskScheduler& scheduler) {
        // The task waits on its own copy of the future: the Gate may be destroyed while the task is still parked (a
        // scheduler drained by its destructor outlives a Gate declared after it). `this` is used only before park()
        // returns.
        const bool accepted = scheduler.enqueue([this, open = m_open_future](const core::ExecutionContext&) {
            m_parked.set_value();
            open.wait();
        });
        CHECK(accepted);
        m_parked_future.wait();
    }
    void open() { m_open.set_value(); }

private:
    std::promise<void> m_parked;
    std::future<void> m_parked_future = m_parked.get_future();
    std::promise<void> m_open;
    std::shared_future<void> m_open_future = m_open.get_future().share();
};

std::shared_ptr<const core::Event> event(const std::string& topic) {
    return std::make_shared<core::Event>(core::Event{topic, std::make_any<int>(1)});
}

// Records which subscriber objects have been destroyed, so a handler can detect being called on one.
std::mutex g_destroyed_mutex;
std::set<const void*> g_destroyed;
std::atomic<int> g_calls_on_destroyed{0};

/// A subscriber that follows the bus contract: subscribes in its constructor, unsubscribes in its destructor.
class Subscriber {
public:
    Subscriber(core::IEventBus& bus, const std::string& topic) : m_bus(bus) {
        m_id = m_bus.subscribe(topic, [this](std::shared_ptr<const core::Event>) { on_event(); });
        CHECK(m_id != 0);
    }
    ~Subscriber() {
        m_bus.unsubscribe(m_id);
        std::lock_guard lock(g_destroyed_mutex);
        g_destroyed.insert(this);
    }
    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;
    std::atomic<int> calls{0};

private:
    void on_event() {
        {
            std::lock_guard lock(g_destroyed_mutex);
            if (g_destroyed.contains(this)) ++g_calls_on_destroyed;
        }
        ++calls;
    }
    core::IEventBus& m_bus;
    core::IEventBus::SubscriptionId m_id{0};
};

// --- Event bus ------------------------------------------------------------------------------------------------------------

void test_normal_delivery_is_preserved() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 2);
    core::InMemoryEventBus bus(scheduler);
    std::atomic<int> async_calls{0}, immediate_calls{0}, other_topic_calls{0};
    const auto a = bus.subscribe("t", [&](std::shared_ptr<const core::Event>) { ++async_calls; });
    const auto b = bus.subscribe("t", [&](std::shared_ptr<const core::Event>) { ++immediate_calls; });
    bus.subscribe("other", [&](std::shared_ptr<const core::Event>) { ++other_topic_calls; });
    CHECK(a != 0 && b != 0 && a != b);
    CHECK(bus.subscriber_count("t") == 2);

    bus.dispatch_immediate(event("t")); // synchronous: both handlers ran before it returns
    CHECK(async_calls == 1 && immediate_calls == 1);

    for (int i = 0; i < 10; ++i) bus.publish(event("t"));
    scheduler.shutdown(); // every accepted delivery has run when this returns
    CHECK(async_calls == 11 && immediate_calls == 11);
    CHECK(other_topic_calls == 0);
    CHECK(logger.errors == 0);
    std::printf("test_normal_delivery_is_preserved passed\n");
}

void test_unsubscribed_handler_never_sees_a_queued_event() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 1);
    core::InMemoryEventBus bus(scheduler);
    std::atomic<int> removed_calls{0}, kept_calls{0};
    const auto removed = bus.subscribe("t", [&](std::shared_ptr<const core::Event>) { ++removed_calls; });
    bus.subscribe("t", [&](std::shared_ptr<const core::Event>) { ++kept_calls; });

    Gate gate;
    gate.park(scheduler);
    bus.publish(event("t")); // queued behind the gate
    bus.publish(event("t"));
    bus.unsubscribe(removed);
    CHECK(bus.subscriber_count("t") == 1);
    gate.open();
    scheduler.shutdown();
    CHECK(removed_calls == 0);
    CHECK(kept_calls == 2);
    std::printf("test_unsubscribed_handler_never_sees_a_queued_event passed\n");
}

void test_destroyed_subscriber_with_a_queued_event() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 1);
    core::InMemoryEventBus bus(scheduler);
    auto doomed = std::make_unique<Subscriber>(bus, "t");
    Subscriber survivor(bus, "t");

    Gate gate;
    gate.park(scheduler);
    for (int i = 0; i < 5; ++i) bus.publish(event("t"));
    doomed.reset(); // destroyed while five deliveries to it are queued
    gate.open();
    scheduler.shutdown();
    CHECK(g_calls_on_destroyed == 0);
    CHECK(survivor.calls == 5);
    CHECK(bus.subscriber_count("t") == 1);
    std::printf("test_destroyed_subscriber_with_a_queued_event passed\n");
}

void test_bus_destroyed_with_pending_events() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 1);
    std::atomic<int> calls{0};
    auto bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    bus->subscribe("t", [&](std::shared_ptr<const core::Event>) { ++calls; });

    Gate gate;
    gate.park(scheduler);
    for (int i = 0; i < 5; ++i) bus->publish(event("t"));
    bus.reset(); // the bus is gone; its five deliveries are still queued
    gate.open();
    scheduler.shutdown(); // runs them: each finds the bus closed and does nothing
    CHECK(calls == 0);
    CHECK(logger.errors == 0);
    std::printf("test_bus_destroyed_with_pending_events passed\n");
}

void test_destroyed_bus_memory_is_never_touched_by_queued_deliveries() {
    // The original defect: a queued delivery captured the bus's `this` and dereferenced it when it ran. Freed memory
    // often still looks valid, so here the bus lives in storage the test controls and is overwritten after destruction:
    // a delivery that still reached the bus object would read garbage and crash.
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 1);
    alignas(core::InMemoryEventBus) std::byte storage[sizeof(core::InMemoryEventBus)];
    auto* bus = new (storage) core::InMemoryEventBus(scheduler);
    std::atomic<int> calls{0};
    bus->subscribe("t", [&](std::shared_ptr<const core::Event>) { ++calls; });

    Gate gate;
    gate.park(scheduler);
    for (int i = 0; i < 5; ++i) bus->publish(event("t"));
    bus->~InMemoryEventBus();
    std::memset(storage, 0xA5, sizeof(storage));
    gate.open();
    scheduler.shutdown();
    CHECK(calls == 0);
    std::printf("test_destroyed_bus_memory_is_never_touched_by_queued_deliveries passed\n");
}

void test_closed_bus_accepts_nothing() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 1);
    core::InMemoryEventBus bus(scheduler);
    std::atomic<int> calls{0};
    bus.subscribe("t", [&](std::shared_ptr<const core::Event>) { ++calls; });
    bus.close();
    bus.close(); // idempotent
    CHECK(bus.subscriber_count("t") == 0);
    CHECK(bus.subscribe("t", [&](std::shared_ptr<const core::Event>) { ++calls; }) == 0);
    bus.publish(event("t"));
    bus.dispatch_immediate(event("t"));
    scheduler.shutdown();
    CHECK(calls == 0);
    std::printf("test_closed_bus_accepts_nothing passed\n");
}

void test_unsubscribe_waits_for_a_running_handler() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 1);
    core::InMemoryEventBus bus(scheduler);
    std::promise<void> entered, release;
    auto entered_future = entered.get_future();
    auto release_future = release.get_future().share();
    std::atomic<bool> handler_finished{false};
    const auto id = bus.subscribe("t", [&](std::shared_ptr<const core::Event>) {
        entered.set_value();
        release_future.wait();
        handler_finished = true;
    });
    bus.publish(event("t"));
    entered_future.wait(); // the handler is running on the worker

    std::atomic<bool> finished_when_unsubscribe_returned{false};
    auto unsubscriber = std::async(std::launch::async, [&] {
        bus.unsubscribe(id);
        finished_when_unsubscribe_returned = handler_finished.load();
    });
    // While the handler is held, unsubscribe() cannot return. (A correct implementation never returns here, so this
    // bounded wait cannot produce a false failure; a broken one is caught with high probability.)
    CHECK(unsubscriber.wait_for(100ms) == std::future_status::timeout);
    release.set_value();
    unsubscriber.wait();
    CHECK(finished_when_unsubscribe_returned); // it returned only after the handler had finished
    CHECK(bus.subscriber_count("t") == 0);
    scheduler.shutdown();
    std::printf("test_unsubscribe_waits_for_a_running_handler passed\n");
}

void test_unsubscribe_is_idempotent_and_ignores_unknown_ids() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 1);
    core::InMemoryEventBus bus(scheduler);
    std::atomic<int> calls{0};
    const auto id = bus.subscribe("t", [&](std::shared_ptr<const core::Event>) { ++calls; });
    const auto other = bus.subscribe("u", [&](std::shared_ptr<const core::Event>) { ++calls; });
    bus.unsubscribe(id);
    bus.unsubscribe(id);
    bus.unsubscribe(0);
    bus.unsubscribe(id + other + 1000);
    CHECK(bus.subscriber_count("t") == 0);
    CHECK(bus.subscriber_count("u") == 1); // only the named subscription went
    bus.dispatch_immediate(event("u"));
    CHECK(calls == 1);
    scheduler.shutdown();
    std::printf("test_unsubscribe_is_idempotent_and_ignores_unknown_ids passed\n");
}

// --- Task scheduler -------------------------------------------------------------------------------------------------------

void test_scheduler_shutdown_with_an_empty_queue_and_repeated() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 3);
    scheduler.shutdown();
    scheduler.shutdown();
    scheduler.shutdown();
    CHECK(logger.errors == 0);
    std::printf("test_scheduler_shutdown_with_an_empty_queue_and_repeated passed\n");
}

void test_scheduler_shutdown_drains_pending_work() {
    CountingLogger logger;
    core::BasicTaskScheduler scheduler(logger, 1);
    Gate gate;
    gate.park(scheduler);
    std::atomic<int> ran{0};
    for (int i = 0; i < 100; ++i) CHECK(scheduler.enqueue([&](const core::ExecutionContext&) { ++ran; }));

    // shutdown() begins while the worker is still parked with 100 tasks pending, and must wait for all of them.
    auto shutting_down = std::async(std::launch::async, [&] { scheduler.shutdown(); });
    gate.open();
    shutting_down.wait();
    CHECK(ran == 100);
    std::printf("test_scheduler_shutdown_drains_pending_work passed\n");
}

void test_scheduler_rejects_work_after_shutdown() {
    CountingLogger logger;
    std::atomic<int> ran{0};
    {
        core::BasicTaskScheduler scheduler(logger, 2);
        scheduler.shutdown();
        const bool accepted = scheduler.enqueue([&](const core::ExecutionContext&) { ++ran; });
        CHECK(!accepted);
        CHECK(logger.warnings == 1); // the rejection stays observable
    } // destruction after shutdown: nothing more happens
    CHECK(ran == 0);
    std::printf("test_scheduler_rejects_work_after_shutdown passed\n");
}

void test_scheduler_destructor_drains_like_shutdown() {
    CountingLogger logger;
    std::atomic<int> ran{0};
    {
        core::BasicTaskScheduler scheduler(logger, 1);
        Gate gate;
        gate.park(scheduler);
        for (int i = 0; i < 10; ++i) CHECK(scheduler.enqueue([&](const core::ExecutionContext&) { ++ran; }));
        gate.open();
    }
    CHECK(ran == 10);
    std::printf("test_scheduler_destructor_drains_like_shutdown passed\n");
}

// --- A real subscriber: the desktop shell ------------------------------------------------------------------------------

void test_shell_destroyed_with_queued_events() {
    auto logger = std::make_shared<CountingLogger>();
    core::BasicTaskScheduler scheduler(*logger, 1);
    auto bus = std::make_shared<core::InMemoryEventBus>(scheduler);
    auto display = std::make_shared<desktop::MockDisplayBackend>(logger);
    auto compositor = std::make_shared<desktop::BasicCompositor>(display, logger);
    auto wm = std::make_shared<desktop::BasicWindowManager>(bus, logger);
    auto toolkit = std::make_shared<desktop::BasicWidgetToolkit>();
    auto decorator = std::make_shared<desktop::BasicWindowDecorator>(toolkit);
    auto taskbar = std::make_shared<desktop::BasicTaskbar>(toolkit);

    const std::vector<std::string> shell_topics = {"notification_show", "notification_dismiss", hal::kTopicInputMouse,
                                                   hal::kTopicInputPointer, hal::kTopicInputKey, hal::kTopicInputText};
    std::atomic<int> probe_calls{0};
    bus->subscribe("notification_show", [&](std::shared_ptr<const core::Event>) { ++probe_calls; });

    auto shell = std::make_unique<apps::DesktopShell>(wm, compositor, toolkit, decorator, taskbar, bus);
    shell->launch();
    for (const auto& topic : shell_topics) {
        CHECK(bus->subscriber_count(topic) == (topic == "notification_show" ? 2u : 1u));
    }

    Gate gate;
    gate.park(scheduler);
    // Deliveries the shell would handle, queued behind the gate.
    desktop::Notification n{1, "title", "body", desktop::NotificationPriority::Normal};
    bus->publish(std::make_shared<core::Event>(core::Event{"notification_show", std::make_any<desktop::Notification>(n)}));
    bus->publish(std::make_shared<core::Event>(core::Event{"notification_dismiss", std::make_any<uint32_t>(1u)}));
    const hal::MouseEvent move(200, 200, 0);
    bus->publish(std::make_shared<core::Event>(core::Event{hal::kTopicInputMouse, std::make_any<hal::MouseEvent>(move)}));

    shell.reset(); // the shell is destroyed while those deliveries are queued
    for (const auto& topic : shell_topics) {
        CHECK(bus->subscriber_count(topic) == (topic == "notification_show" ? 1u : 0u));
    }
    gate.open();
    scheduler.shutdown(); // the queued deliveries run now: none reaches the destroyed shell
    CHECK(probe_calls == 1); // other subscribers still receive
    CHECK(logger->errors == 0);
    std::printf("test_shell_destroyed_with_queued_events passed\n");
}

} // namespace

int main() {
    std::printf("Running shutdown safety tests...\n");
    test_normal_delivery_is_preserved();
    test_unsubscribed_handler_never_sees_a_queued_event();
    test_destroyed_subscriber_with_a_queued_event();
    test_bus_destroyed_with_pending_events();
    test_destroyed_bus_memory_is_never_touched_by_queued_deliveries();
    test_closed_bus_accepts_nothing();
    test_unsubscribe_waits_for_a_running_handler();
    test_unsubscribe_is_idempotent_and_ignores_unknown_ids();
    test_scheduler_shutdown_with_an_empty_queue_and_repeated();
    test_scheduler_shutdown_drains_pending_work();
    test_scheduler_rejects_work_after_shutdown();
    test_scheduler_destructor_drains_like_shutdown();
    test_shell_destroyed_with_queued_events();
    std::printf("All shutdown safety tests passed.\n");
    return 0;
}
