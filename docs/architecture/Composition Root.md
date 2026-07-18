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
The initialization and execution happen in three distinct phases:
1. **Construction:** Dependencies are instantiated via `std::make_shared` and `std::make_unique`. Foundational dependencies like the `[[FileLogger]]` are constructed first and injected into consumers like the `TaskScheduler` and `[[BasicWindowManager]]`. 
2. **Initialization (`initialize()`):** Invokes `.initialize()` on hardware abstractions (Display, Audio, Compute, Clock, Block, Network) and launches system applications (Desktop Shell, Terminal, File Explorer, Demo App).
3. **Execution (`run()`):** Starts the `ServiceManager` and runs the main event loop. This loop polls the input drivers and IPC framework, triggers application frame updates, and finally pushes the z-ordered rendering commands to the `[[BasicCompositor]]`. A `[[FrameClock]]` regulates this loop to a steady 60 FPS.

## Ownership & Lifetime Management
Memory management heavily leverages smart pointers. The `CompositionRoot` holds long-lived `std::shared_ptr` references to most system components. This shared ownership model is essential because many components hold references to each other (e.g., applications holding references to the `WindowManager` and `EventBus`). 

### Shutdown Lifecycle vs. Object Destruction
The `CompositionRoot` explicitly separates the **logical shutdown sequence** from **C++ object destruction (RAII)**.

#### 1. Logical Shutdown Sequence (`shutdown()`)
This operational phase gracefully halts active processing in strict dependency order:
1. **Consumers:** High-level managers (like `ServiceManager`) are signaled to stop.
2. **Hardware Abstractions:** Drivers (like Display, Network, BlockDevice, HardwareClock) are halted to cut off hardware interrupts and events.
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
