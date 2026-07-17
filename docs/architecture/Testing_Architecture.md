# Testing Architecture

## 1. Overview
Automated testing is a first-class architectural component of VynexOS. A rigid, modular regression testing strategy ensures that as the operating system scales in complexity (e.g., from mock HAL components to physical SDL2 drivers), existing behaviors remain strictly preserved without introducing unexpected side-effects.

## 2. Test Suite Responsibilities
The testing structure is divided into independently executing binaries to enforce isolation and prevent cross-contamination of global OS state. The following test suites are formally established:

- **CoreTests:** General OS fundamentals, standard library abstractions, data structures.
- **HALTests:** Hardware Abstraction Layer compliance. Validates the Storage, Network, and Clock devices independent of the core runtime.
- **PluginManagerTests:** Validates dynamic library loading, ABI enforcement, version mismatch rejection, and dependency injection via `PluginContext`.
- **SchedulerTests:** Exercises the asynchronous `TaskScheduler`, verifying thread pool semantics and task execution determinism.
- **EventBusTests:** Validates IPC messaging and synchronous/asynchronous event dispatch.
- **ConfigurationTests:** Verifies parsing, reading, and writing to the system configuration tree.
- **IntegrationTests:** Broad, system-wide bootstrap and teardown workflows.

## 3. Coverage & Regression Strategy
- Every newly introduced subsystem **must** be accompanied by automated tests before a development milestone is considered complete.
- All testing executables must execute sequentially via CMake's `CTest`.
- Executables are designed to fail-fast. If an `ASSERT_TRUE` fails, the test suite increments an internal failure counter and returns a non-zero exit code to halt the CI/CD pipeline.
- All test suites must compile natively under MSVC with `/WX` (treat warnings as errors) and `/W4` (level 4 warnings) to guarantee code cleanliness.

## 4. Verification Workflow

### Running Individual Tests
To run an individual test suite without CTest, execute the binary directly from the build output directory (e.g., `bin/Debug`):
```cmd
cd build\bin\Debug
.\test_hal.exe
```

### Running All Tests via CTest
The primary verification entry point for the entire VynexOS codebase is CTest. To execute all registered test suites automatically:
1. Navigate to the build directory.
2. Execute:
   ```cmd
   ctest -C Debug --output-on-failure
   ```

### Expected Output
When running CTest, it executes each suite independently and reports a PASS/FAIL status. A successful run looks like:
```text
1/3 Test #1: SchedulerTests ...................   Passed    1.50 sec
2/3 Test #2: PluginManagerTests ...............   Passed    0.62 sec
3/3 Test #3: HALTests .........................   Passed    1.15 sec

100% tests passed, 0 tests failed out of 3
```
If a test fails, `--output-on-failure` ensures that the standard output (containing the `[FAIL]` reason) is printed directly to the console.

### Release Verification Workflow
Before any Version release is considered complete, the codebase must pass the **Release Verification Workflow**. This is a strict sequence documented in the [Review Checklist](../Developer Guide/Review Checklist.md) that guarantees zero regressions via full CTest execution.

## 5. Adding New Tests
1. **Identify the Target Suite:** Determine if the feature is a core component, a HAL driver, or a Plugin. If an appropriate suite does not exist, scaffold a new test executable in `src/tests`.
2. **Implement Tests:** Use standard C++ alongside `vynexos::core::FileLogger` to record test execution. Maintain zero side-effects on disk unless testing `IBlockDevice`.
3. **Register Target:** Add the executable to `src/tests/CMakeLists.txt` via `add_executable()` and `add_test()`. Link only the necessary VynexOS internal libraries (e.g., `vynex_core`, `vynex_hal`).
4. **Enforce Determinism:** Verify that the test cleans up all allocated memory and instantiated tasks prior to termination.
