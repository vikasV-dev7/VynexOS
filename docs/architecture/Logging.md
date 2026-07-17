# Logging Architecture

## 1. Overview
VynexOS features a highly configurable, thread-safe logging infrastructure through the `vynexos::core::ILogger` interface. Logging provides deterministic insight into the asynchronous task scheduler, hardware abstraction lifecycles, and plugin manager state.

## 2. Configurable Log Levels
The logging infrastructure exposes a strictly defined hierarchy of verbosity, governed by the `vynexos::core::LogLevel` enumeration:

1. **TRACE**: Highly verbose step-by-step diagnostic information (e.g., individual frame rendering or raw packet parsing).
2. **DEBUG**: Diagnostic information for developers (e.g., worker thread instantiation, module lifecycles).
3. **INFO**: General operational milestones (e.g., OS bootstrap sequence, successful subsystem initialization). This is the default active level.
4. **WARN**: Handled, but unexpected conditions that might indicate degradation or configuration issues.
5. **ERROR**: Recoverable errors that halted a specific subsystem or task (e.g., out-of-bounds storage read).
6. **FATAL**: Critical failures indicating impending system crash.
7. **CRITICAL**: Immediate and absolute system corruption resulting in a halt.

## 3. Configuration & Test Runner Defaults
The active logging level dictates the minimum severity that will be emitted. Any log strictly below the active level is gracefully discarded with zero locking overhead.

### Default Behaviors
- **Development Runtime:** `INFO`
- **Automated Test Suites:** `INFO`
- **Verbose Debugging:** `DEBUG` or `TRACE`

Because the test suites execute within the CI pipeline via CTest, the default level is `INFO` to suppress unnecessary noise (like `[DEBUG] BasicTaskScheduler: Worker thread X started.`). If detailed diagnostics are required, the test runner can dynamically configure the logger:

```cpp
auto logger = std::make_shared<core::FileLogger>("hal_test.log");
// Enable verbose debugging explicitly
logger->set_log_level(core::LogLevel::Debug); 
```

## 4. Subsystem Guidelines
- **Core OS / Scheduler**: Should predominantly log at `DEBUG` for internal state transitions.
- **HAL Drivers**: Should log initialization status at `INFO`. Reads/writes and hardware communication loops should strictly be `TRACE` to avoid I/O bottlenecks.
- **Plugins**: Should log via their injected `PluginContext` at `INFO` or `WARN` when appropriate. Plugin developers should avoid excessive logging that competes with the host kernel thread.
