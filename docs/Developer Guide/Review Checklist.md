# Release Review Checklist

Before any major Version feature is merged and formally tagged for release, the following checklist must be strictly verified.

## Pre-Release Requirements

- [ ] **Clean Configure**: Delete the build directory and run `cmake ..` with no configuration errors.
- [ ] **Clean Build**: Run `cmake --build .` from a clean state.
- [ ] **Zero Compiler Warnings**: The build must complete with `0 Warnings` and `0 Errors` under `/WX` and `/W4` MSVC flags.
- [ ] **Automated Tests**: Execute `ctest -C Debug --output-on-failure`. All tests must pass with exit code 0.
- [ ] **Runtime Verification**: Launch `vynex_init.exe` manually to verify graphical initialization and deterministic HAL boot order visually.
- [ ] **Documentation**: Ensure the Obsidian vault (`docs/architecture/`) is fully updated with architectural decisions, diagrams, and Development Diary entries.
