# Changelog

## [Unreleased] - Version 0.6.0
### Added
- Native `[[ISurface]]` rendering pathway in `[[DesktopShell]]` eliminating all legacy WindowBuffer abstractions.
- `[[BasicWidgetToolkit]]` now supports true intra-window alpha blending formulas.
- Unit Test Stabilization documentation: `[[Version_0.6.0_Test_Stabilization]]`.

### Changed
- Refactored all tests to strictly validate `SoftwareSurface` alpha blending mathematically.
- Purged all proxy interfaces referencing the legacy `submit_buffer` architecture.

### Fixed
- **Compositor Bug**: Addressed ghosting issue by mathematically fixing compositor `BlendPixel` logic, removing double pre-multiplication.
- **Architectural Violation (INC-0004)**: Fixed an encapsulation violation in `test_runtime_bug.cpp` that attempted to access private internals of `CompositionRoot`. Test now gracefully consumes public interface via `m_input_driver` casting.
- **Renderer Link Error**: Avoided `std::min` with initializer lists in `basic_compositor.cpp` to prevent MSVC `__std_min_element_4i` unresolved symbol errors.

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

## [v0.6.0 - Ongoing]
- **Phase 1 Complete:** Runtime Lifecycle Completion. `CompositionRoot` orchestrates graceful termination via `IInputDriver` polling and performs strict reverse-order destruction. Passed full engineering verification gate.
