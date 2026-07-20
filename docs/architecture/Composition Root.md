<!-- AUTO-GENERATED START -->
# Composition Root

## Composition Root
```mermaid
graph LR title Composition Root Dependency Graph
    CR[Composition Root]
    EventBus
    WindowManager
    [[DesktopShell]]
    AudioDriver
    CR --> EventBus
    CR --> WindowManager
    CR --> [[DesktopShell]]
    CR --> AudioDriver
```
<!-- AUTO-GENERATED END -->

## Purpose & Design Goals
The `CompositionRoot` is the master bootstrapper for VynexOS. Its primary goal is to wire together all core services, drivers, and application shells into a coherent, functioning system. It implements the Dependency Injection principle known as the "Composition Root", ensuring that components (like the Event Bus, Display Backend, and Window Manager) remain decoupled and testable while still being properly instantiated and connected in a centralized location.

## Internal Implementation Overview
## Responsibilities

1. **Service Instantiation:** Creates concrete instances of core services (TaskScheduler, EventBus, FileLogger).
2. **Platform Abstraction (HAL):** Instantiates the correct Hardware Abstraction Layer implementations depending on the target platform (e.g., SDL2 vs Mock).
3. **Dependency Injection:** Wires dependencies together (e.g., passing the EventBus to the WindowManager).
4. **Application Registration:** Registers IApplication factories into the ApplicationRegistry.
5. **Runtime Coordination (Delegated):** Launches the `DesktopShell` and delegates permanent runtime execution (update/render loops) to it.

> [!NOTE]
> As of Version 0.8.0.1, the `CompositionRoot` no longer contains the application `update` or `render` loop. It acts strictly as a bootstrapper and delegates runtime execution entirely to the `DesktopShell`.

## Logical Shutdown Sequence

The `shutdown()` method enforces strict dependency-ordered teardown:

1. High-level services are stopped via `ServiceManager`.
2. Hardware drivers and Platform HALs are shut down.
3. **Desktop Runtime Teardown:** The `DesktopShell` is instructed to clear all active `RunningApplication` descriptors to safely release their resources.
4. Active `IApplications` are terminated via `ApplicationRegistry`.
5. Core infrastructure (Event Bus, Task Scheduler) is terminated.
3. **Task Scheduler:** The `[[ITaskScheduler]]` is explicitly shutdown **last**. It blocks, drains its queues, and executes residual cleanup logic enqueued by the dying subsystems.

#### 2. Object Destruction Order (RAII)
After `shutdown()` completes, C++ naturally releases memory via RAII (reverse order of declaration). To ensure memory safety during this final teardown phase, class members are strictly ordered. Foundational infrastructure (`TaskScheduler`, `ConfigManager`, `Logger`) is declared first so that it safely outlives the consumers (Apps, `EventBus`, Services) that might hold dangling references during destruction. Explicit manual `.reset()` calls are avoided in favor of deterministic C++ RAII behavior.

## Thread Safety & Execution Context
The `CompositionRoot` is inherently single-threaded in its orchestration. The main event loop (`run()`) executes on the primary boot thread. While it spins up the `TaskScheduler` which internally manages a worker thread pool, the `CompositionRoot` itself accesses member variables without locks. It is expected that methods like `initialize()`, `run()`, and `shutdown()` are called sequentially from the main thread. 

## Failure Handling & Error Recovery
During the `initialize()` phase, if critical backends (like the Display Backend) fail to initialize, the `CompositionRoot` logs a fatal error. Less critical failures (e.g., Audio Backend, Compute Backend) log an error but allow the boot sequence to continue, demonstrating a resilient "degraded mode" capability.
If `run()` encounters a boot failure from the `ServiceManager`, the system immediately aborts the run loop.

## Extension Points & Future Roadmap
- **Dynamic Module Loading:** The V1.0 architecture introduced `[[BasicPluginManager]]` and `[[PluginContextFactory]]`. Future iterations will expand this to support dynamically loaded, hot-swappable system modules.
- **Dependency Graph Validation:** Currently, the dependency graph is hardcoded in the constructor. A future roadmap item includes declarative configuration-based wiring to allow entirely different system topologies without recompiling the `CompositionRoot`.
