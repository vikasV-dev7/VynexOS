<!-- AUTO-GENERATED START -->
# Changelog

## [Unreleased] - Version 0.6.0
### Planned
- Event routing optimization.
- Hardware-accelerated UI compositing (Vynex UI Layer).

## [v0.5.0] - 2026-07-18
### Added
- `SDL2DisplayBackend` utilizing strict RAII and PIMPL.
- `SDL2InputDriver` mapping physical SDK inputs to VynexOS topics.
- `FrameClock` chronometer ensuring capped 60 FPS update logic.
- Automated `SDLIntegrationTests` test suite tracking frame timing and event bursting.
- Physical Driver Layer documentation (`0.5.0_Physical_Driver_Layer.md`).
- Technical Debt tracking via `Technical Debt.md` (TD-0001).

### Changed
- Refactored `CompositionRoot` to allocate SDL contexts dynamically inside the hardware thread pool execution.

## [v0.4.1] - 2026-07-17
### Added
- `vynexos::core::ILogger` dynamically configurable logging levels.
- Dedicated `HALTests` verification executable.
- Formal Obsidian documentation for Testing and Logging Architecture.

### Changed
- Unified all verification suites under CTest.
- `CompositionRoot` defaults to `INFO` logging, suppressing noisy `TRACE` and `DEBUG` events during test execution.

### Removed
- Phantom "System" notifications simulating hardware input in the `CompositionRoot::run()` loop.

- **v1.0**: Runtime Architecture 1.0 Complete.
<!-- AUTO-GENERATED END -->
