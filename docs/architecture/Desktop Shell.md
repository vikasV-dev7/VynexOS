<!-- AUTO-GENERATED START -->
The `DesktopShell` (implementing `IDesktopShell`) is the permanent runtime owner for the VynexOS desktop environment. It provides the visual shell (Taskbar, Start Menu, System Tray) and orchestrates the execution of all running applications.

## Responsibilities

1. **Runtime Ownership:** Maintains the collection of `RunningApplication` descriptors, explicitly tracking the linkage between an `IApplication`, its `ISurface`, and its `window_id`.
2. **Execution Pipeline:** Drives the `update()` and `render()` loops for all active applications.
3. **Event Routing:** Receives global input events from the `EventBus` and routes them deterministically: mouse events to the hit-tested target window, and keyboard events exclusively to the focused application.
4. **Focus Ownership:** Determines which `RunningApplication` currently owns runtime focus.
5. **Drag Orchestration:** Owns transient interaction state (`DragState`) and coordinates pointer capture for interactive window movement.
6. **Visual Interface:** Renders the system UI elements (Taskbar, Launcher).
7. **State Management:** Handles window visibility, minimization, and dirty rect optimization at the desktop level.

> [!IMPORTANT]
> The DesktopShell handles *runtime* coordination, including exclusive focus tracking via `focus_window()` and `clear_focus()`. The instantiation, memory ownership, and factory registration of `IApplications` remains the explicit responsibility of the `ApplicationRegistry`.

## Runtime Model

The shell manages applications using the `RunningApplication` struct:

```cpp
struct RunningApplication {
    IApplication* app = nullptr;
    uint32_t window_id = 0;
    std::shared_ptr<ISurface> surface;
    bool visible = true;
    bool focused = false;
};
```

This ensures that applications remain entirely ignorant of their windowing context while allowing the Desktop Shell to intelligently pause rendering for hidden windows.
<!-- AUTO-GENERATED END -->
