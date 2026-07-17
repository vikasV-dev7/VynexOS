<!-- AUTO-GENERATED START -->
# Changelog

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
