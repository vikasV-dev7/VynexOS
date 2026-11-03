// Shared check macro for the test executables.
//
// Why this exists: <cassert>'s assert() expands to nothing when NDEBUG is defined (Release). Tests written with assert()
// therefore lose both their checks AND any call placed inside the expression: a test could crash (it then used results of
// calls that never ran) or pass while exercising nothing. CHECK() is the replacement for every test check:
//   - the expression is ALWAYS evaluated, in every build configuration;
//   - a failure prints the expression, file, line and an optional context string, then terminates the process with exit
//     code 1 immediately (no abort(), so no crash dialog or JIT-debugger delay) and CTest reports the test as failed.
// Use it for checks; still keep operations that change state in their own statements when that reads better:
//     const bool launched = registry.launch("app");   // the operation
//     CHECK(launched);                                 // the verification
// A CTest source-hygiene test fails the build if assert( reappears in a test source.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

namespace vynex_test {

/// Optional description of what the test is currently doing; printed with a failing CHECK.
inline std::string& context() {
    static std::string text;
    return text;
}

[[noreturn]] inline void check_failed(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "CHECK failed: %s\n  at %s:%d\n", expression, file, line);
    if (!context().empty()) std::fprintf(stderr, "  context: %s\n", context().c_str());
    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(1);
}

} // namespace vynex_test

#define CHECK(...)                                                                    \
    do {                                                                              \
        if (!(__VA_ARGS__)) ::vynex_test::check_failed(#__VA_ARGS__, __FILE__, __LINE__); \
    } while (false)
