<!-- AUTO-GENERATED START -->
# Window Manager

## Window Manager
State management for windows.
<!-- AUTO-GENERATED END -->

## Purpose & Design Goals
The `BasicWindowManager` (implementing `IWindowManager`) is the core spatial compositor and layout engine for VynexOS. 

## Responsibilities

1. **Window Lifecycle:** Creates and destroys window IDs (`uint32_t`) and their backing `WindowState`.
2. **Spatial Geometry:** Tracks the X, Y, Width, and Height of all windows.
3. **Hit Testing:** Encapsulates the logic to determine which window occupies a specific screen coordinate via `get_window_at(x, y)`.
4. **Z-Ordering:** Maintains the back-to-front rendering stack.
5. **Scene Graph Generation:** Produces the `SceneGraph` structure consumed by the compositor.

> [!CAUTION]
> The WindowManager **does not** own runtime focus. While it tracks Z-order, it is the `DesktopShell`'s responsibility to command focus changes and route input. The `WindowManager` merely answers spatial queries (`get_window_at`).

## Hit Testing & Focus Architecture

VynexOS strictly separates spatial geometry from runtime focus:
1. `DesktopShell` receives a raw `HAL_INPUT_MOUSE` event.
2. `DesktopShell` asks `IWindowManager::get_window_at(x, y)` which window was clicked.
3. `IWindowManager` iterates its internal Z-order from top to bottom, checking bounding boxes against visible windows, and returns a `std::optional<uint32_t>`.
4. `DesktopShell` then explicitly commands `IWindowManager::focus_window(id)` to update the Z-order, updates its internal `RunningApplication` focus metadata, and routes the mouse event.

## Ownership & Lifetime Management
The Window Manager does not "own" the actual graphical buffers or rendering contexts—those belong to the Compositor and the Widget Toolkit. It purely owns the abstract metadata (IDs, titles, coordinates). IDs are leased out upon `create_window()` and must be explicitly relinquished via `destroy_window()`.

## Thread Safety & Execution Context
The manager is fully thread-safe. All public API mutations (`create_window`, `move_window`, `resize_window`, etc.) and internal state queries are protected by a mutable `std::mutex`. This is critical because mouse events from the Event Bus arrive asynchronously on task scheduler worker threads, while applications might concurrently request geometry changes from the main event loop.

## Failure Handling & Error Recovery
API boundaries employ robust defensive programming:
- Invalid or already-destroyed window IDs passed to `move_window`, `resize_window`, or `focus_window` are safely ignored via `contains()` or `find()` checks on the internal map.
- If a destroyed window was currently focused, the manager automatically fallback-focuses the next highest window in the z-order stack.

## Testing Strategy
Due to its isolation from rendering, `[[BasicWindowManager]]` can be unit-tested purely through state inspection. Tests verify that synthetic mouse click payloads dispatched to a mock Event Bus correctly mutate the z-order array.

## Extension Points & Future Roadmap
- **Window Decorations:** Future versions will track client-side versus server-side decorations to better handle drag-and-drop window moving.
- **Spatial Partitioning:** The current reverse-iteration hit-testing for mouse clicks is $O(N)$ with the number of windows. As the window count grows, this will be migrated to a quad-tree or similar spatial partitioning structure for performance.
