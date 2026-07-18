<!-- AUTO-GENERATED START -->
# Window Manager

## Window Manager
State management for windows.
<!-- AUTO-GENERATED END -->

## Purpose & Design Goals
The `[[BasicWindowManager]]` serves as the centralized authority for tracking window states, geometries, and z-ordering in the VynexOS desktop environment. It decouples window manipulation logic from rendering logic, providing a stable API for application shells and compositors to query and mutate window visibility.

## Internal Implementation Overview
Window state is managed via an internal `WindowState` struct, containing the title and `WindowGeometry`. Windows are stored in a `std::unordered_map` keyed by a unique, monotonically increasing `uint32_t` ID. 
Z-ordering is maintained using a `std::vector<uint32_t>`, where the front of the vector represents the bottom-most window, and the back represents the top-most (focused) window.

### Focus Routing
The manager integrates directly with the hardware abstraction layer by subscribing to `HAL_INPUT_MOUSE` on the Event Bus. When a mouse click occurs, it iterates the z-order vector in reverse (top to bottom). The first window whose geometry encloses the click coordinates is immediately promoted to the top of the z-order vector and marked as the focused window.

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
