# Changelog

## [v0.8.0] - 2026-07-22
### Implementation 0.8.0.7 — Taskbar & Window Switching
- Introduced `ITaskbar` and `BasicTaskbar` to render running application status.
- Added `TaskbarEntry` model to support decoupled UI synchronization.
- Added `get_window_snapshots()` API in `IWindowManager` for creation-ordered, O(1) taskbar enumeration.
- Implemented active window visual highlighting in the taskbar.
- Implemented click-to-restore behavior for minimized windows.
- Desktop routing integration to ferry taskbar interactions to the `BasicWindowManager`.

### Implementation 0.8.0.6 — Window State Management
- Introduced persistent window state (`Normal`, `Minimized`) owned by `BasicWindowManager`.
- Added visual `MinimizeButton` to `BasicWindowDecorator` and extended `DecorationHitRegion`.
- `DesktopShell` dynamically transfers focus to the next visible window when the active window is minimized.
- Ensured minimized windows are strictly excluded from scene graph rendering, input routing, and hit testing, while `RunningApplication` lifecycle continues.
- Implemented comprehensive unit testing for state lifecycle and focus transfer in `test_desktop_shell.cpp`.

### Implementation 0.8.0.4 — Window Resizing Framework
- Introduced interactive window resizing orchestration within `DesktopShell`.
- Introduced `ResizeState` to manage pointer capture and movement deltas cleanly separated from drag interactions.
- Extended `IWindowManager` with `get_resize_handle_at()` to abstract hit-testing logic from the shell, returning the new `HitTestResult` and `ResizeEdge` types.
- Extended `IWindowManager` with `set_window_geometry()` to allow atomic positional and dimensional updates.
- Centralized minimum window constraint enforcement within `BasicWindowManager`.
- Verified deterministic focus tracking and exclusive event routing during resize captures.
- Full regression suite verified against all interactive desktop primitives.

### Implementation 0.8.0.3 — Window Movement & Drag Operations orchestrated via `DesktopShell`.
- Threshold-based pointer capture to differentiate rapid clicks from window drags.
- Exclusive Mouse Event routing to dragged windows during pointer capture.
- MSVC-compliant strict warning configuration (C++23 compliant).

### Changed
- `DesktopShell` dynamically pushes layout updates via `IWindowManager::move_window` without recalculating internal geometries.
- Re-architected `test_desktop_shell.cpp` regression testing to properly handle explicit `HAL_INPUT_MOUSE` release events.

## [v0.6.0] - 2026-07-19
### Added
- Native `[[ISurface]]` rendering pathway in `[[DesktopShell]]` eliminating all legacy WindowBuffer abstractions.
- `[[BasicWidgetToolkit]]` now supports true intra-window alpha blending formulas and actual 8x8 bitmap text rendering.
- Unit Test Stabilization documentation: `[[Version_0.6.0_Test_Stabilization]]`.
- Debugging history for MSVC optimizations, input determinism, and rendering defects.

### Changed
- Refactored all tests to strictly validate `SoftwareSurface` alpha blending mathematically.
- Purged all proxy interfaces referencing the legacy `submit_buffer` architecture.
- Refactored SDL2 Input queue state tracking for deterministic mouse clicks (fixing launcher/focus thrashing).
- Optimized compositor inner loop using raw pointer arithmetic and memory banding `std::memcpy`, resolving severe MSVC bounds-checking overhead and increasing FPS by 338%.

### Fixed
- **Compositor Bug**: Addressed ghosting issue by mathematically fixing compositor `BlendPixel` logic, removing double pre-multiplication.
- **Architectural Violation (INC-0004)**: Fixed an encapsulation violation in `test_runtime_bug.cpp` that attempted to access private internals of `CompositionRoot`. Test now gracefully consumes public interface via `m_input_driver` casting.
- **Renderer Link Error**: Avoided `std::min` with initializer lists in `basic_compositor.cpp` to prevent MSVC `__std_min_element_4i` unresolved symbol errors.
- **Runtime Lifecycle**: `CompositionRoot` now orchestrates graceful termination via `IInputDriver` polling and performs strict reverse-order destruction.
- **Compositor Allocation Bug**: Eliminated massive 8.29MB per-frame dynamic allocation overhead in `BasicCompositor`.

## [v0.5.0] - 2026-07-18
### Added
- `[[SDL2DisplayBackend]]` utilizing strict RAII and PIMPL.
- `[[SDL2InputDriver]]` mapping physical SDK inputs to VynexOS topics.
- `[[FrameClock]]` chronometer ensuring capped 60 FPS update logic.
- Automated `SDLIntegrationTests` test suite tracking frame timing and event bursting.
- Physical Driver Layer documentation (`0.5.0_Physical_Driver_Layer.md`).
- Technical Debt tracking via `Technical Debt.md` (TD-0001).

### Changed
- Refactored `CompositionRoot` to allocate SDL contexts dynamically inside the hardware thread pool execution.

## [v0.4.1] - 2026-07-17
### Added
- `vynexos::core::[[ILogger]]` dynamically configurable logging levels.
- Dedicated `HALTests` verification executable.
- Formal Obsidian documentation for Testing and Logging Architecture.

### Changed
- Unified all verification suites under CTest.
- `CompositionRoot` defaults to `INFO` logging, suppressing noisy `TRACE` and `DEBUG` events during test execution.

### Removed
- Phantom "System" notifications simulating hardware input in the `CompositionRoot::run()` loop.

- **v1.0**: Runtime Architecture 1.0 Complete.

## [Post-v0.5.0] - Engineering Documentation
- Documented Visual Studio IntelliSense False Positive Investigation.
- Established engineering philosophy regarding compiler authority.
- Began planning for v0.6.0.


