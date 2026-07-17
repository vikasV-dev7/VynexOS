<!-- AUTO-GENERATED START -->
# Development Diary

## v0.4.1 - Engineering Stabilization
**Context & Goal:** After successfully establishing the Version 0.4.0 Hardware Abstraction Layer, we needed to stabilize the testing pipeline before introducing physical driver dependencies.

**What was implemented:**
1. Unified testing via CTest.
2. `HALTests` executable for deterministic block, network, and clock testing.
3. `LogLevel` configuration inside `ILogger`.
4. Extracted demo scaffolding from `CompositionRoot`.

**Why:** To establish a rigorous `ctest -C Debug --output-on-failure` standard that guarantees zero regressions moving into physical development (v0.5.0).

**Root Cause Analyses:**
- **LNK2019 Error during HALTests:** The `CompositionRoot` implementation was previously locked inside the `vynex_init` executable target. Adding `composition_root.cpp` to the `test_hal` target resolved this, verifying RAII teardown correctness.
- **Infinite Test Notifications:** `CompositionRoot::run()` was hardcoded with a 50ms sleep and a `ticks` counter that simulated mock clicks and notifications. This was cleanly removed to allow the runtime to naturally idle.and reflections.

Notes and reflections.
<!-- AUTO-GENERATED END -->
