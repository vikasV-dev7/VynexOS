<!-- AUTO-GENERATED START -->
# Composition Root

## Composition Root
```mermaid
graph LR title Composition Root Dependency Graph
    CR[Composition Root]
    [[EventBus]]
    [[WindowManager]]
    [[DesktopShell]]
    AudioDriver
    CR --> [[EventBus]]
    CR --> [[WindowManager]]
    CR --> [[DesktopShell]]
    CR --> AudioDriver
```
<!-- AUTO-GENERATED END -->
## Shutdown Lifecycle vs. Object Destruction

The `[[CompositionRoot]]` explicitly separates the **logical shutdown sequence** from **C++ object destruction (RAII)**.

### 1. Logical Shutdown Sequence (`shutdown()`)
This operational phase gracefully halts active processing in strict dependency order:
1. **Consumers:** High-level managers (like `ServiceManager`) are signaled to stop.
2. **Hardware Abstractions:** Drivers (like Display) are halted to cut off hardware interrupts and events.
3. **Task Scheduler:** The `[[ITaskScheduler]]` is explicitly shutdown **last**. It blocks, drains its queues, and executes residual cleanup logic enqueued by the dying subsystems.

### 2. Object Destruction Order (RAII)
After `shutdown()` completes, C++ naturally releases memory via RAII (reverse order of declaration). To ensure memory safety during this final teardown phase, class members are strictly ordered. Foundational infrastructure (`TaskScheduler`, `ConfigManager`, `Logger`) is declared first so that it safely outlives the consumers (Apps, `[[EventBus]]`, Services) that might hold dangling references during destruction. Explicit manual `.reset()` calls are avoided in favor of deterministic C++ RAII behavior.
