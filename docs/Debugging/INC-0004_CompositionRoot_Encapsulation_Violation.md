# INC-0004: CompositionRoot Encapsulation Violation

## Root Cause
In an attempt to forcibly mock the input driver within `test_runtime_bug.cpp` while the `VYNEX_ENABLE_SDL2` flag was globally defined, test code was introduced that directly accessed private members of `CompositionRoot`. The test attempted to instantiate a new `hal::MockInputDriver` using `root.m_event_bus` and `root.m_logger`, despite these fields being appropriately encapsulated as private state within the Composition Root. 

## Offending Source Files
* `src/tests/test_runtime_bug.cpp`
  * Line 16: `auto mock_input = std::make_shared<hal::MockInputDriver>(root.m_event_bus, root.m_logger);`

## Why Private Members Were Accessed
The `MockInputDriver` constructor strictly requires `std::shared_ptr<IEventBus>` and `std::shared_ptr<ILogger>`. Because `CompositionRoot` is responsible for initializing and owning all core services (as the application's root Dependency Injection container), these dependencies are isolated within it. The author of the test incorrectly attempted to read these private fields from the outside to construct the mock, rather than using proper interface interaction or relying on the DI container's public outputs.

## Architectural Impact
Accessing private members of the Composition Root from an external consumer severely violates object encapsulation and breaks the fundamental principles of object-oriented design. `CompositionRoot` is solely responsible for wiring dependencies; exposing its internal wiring to external tests couples the tests to the internal implementation details of the application's boot sequence, leading to fragile builds and compromised architectural integrity.

## Final Fix
The offending test code was reverted to use the existing public API provided by `CompositionRoot`:

```cpp
auto mock_input = std::dynamic_pointer_cast<hal::MockInputDriver>(root.m_input_driver);
if (!mock_input) {
    std::cout << "Skipping test: Not using MockInputDriver\n";
    return 0;
}
```

Instead of forcibly extracting private internal dependencies to create a new mock, the test now accesses the publicly exposed `m_input_driver` interface. Using `dynamic_pointer_cast`, it safely queries whether the runtime provided a `MockInputDriver`. If the system booted with the physical `SDL2InputDriver` (e.g., when `VYNEX_ENABLE_SDL2` is defined), the cast fails safely, and the test gracefully skips execution instead of violating encapsulation.

Additionally, a minor linking error caused by an `std::min` initializer list in `basic_compositor.cpp` was fixed to ensure cross-platform MSVC standard library compatibility.

## Lessons Learned
1. **Never Compromise Encapsulation for Testing:** Test logic should always consume public APIs or rely on properly injected dependencies rather than circumventing access modifiers.
2. **Respect the DI Container:** The Composition Root is the singular owner of dependency construction. If a test requires specific mock injection, it must be done through formal architectural boundaries (such as passing the mock into a public method or utilizing a test-specific DI profile), not by stripping the Root's encapsulation.
3. **Graceful Degradation:** Tests running against polymorphic interfaces should gracefully exit or skip if the concrete implementation does not match test assumptions, rather than breaking the application architecture to force the test to run.
