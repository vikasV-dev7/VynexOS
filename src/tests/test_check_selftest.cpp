// Self-test of the CHECK mechanism, run by CTest in every configuration (including Release with NDEBUG).
//   --pass          every kind of true condition passes and operations inside CHECK really execute
//   --fail          a false CHECK must terminate the process with a non-zero exit code (registered with WILL_FAIL)
//   --fail-side-effect  an operation inside CHECK runs, and then a false CHECK must still fail (registered with WILL_FAIL)
// Together they prove that intentionally broken conditions are really detected and that nothing depends on NDEBUG.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "test_check.hpp"

namespace {

int g_calls = 0;
bool operation() {
    ++g_calls;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "--pass";

    if (std::strcmp(mode, "--fail") == 0) {
        std::printf("selftest: a false CHECK must now terminate this process\n");
        CHECK(1 + 1 == 3);
        std::printf("selftest: ERROR - execution continued past a false CHECK\n");
        return 0; // reaching this is itself wrong; WILL_FAIL turns the resulting exit code 0 into a CTest failure
    }

    if (std::strcmp(mode, "--fail-side-effect") == 0) {
        CHECK(operation());           // executes
        if (g_calls != 1) return 0;   // (would mean the operation did not run; exit 0 = test failure under WILL_FAIL)
        std::printf("selftest: operation ran; now a false CHECK must terminate\n");
        CHECK(operation() && g_calls == 99);
        return 0;
    }

    // --pass
    int n = 0;
    CHECK(++n == 1);                                   // the expression is evaluated even under NDEBUG
    if (n != 1) return 2;
    CHECK(operation());
    CHECK(g_calls == 1);
    CHECK(std::string("a") + "b" == "ab" && "commas and strings are fine");
    auto p = std::make_unique<int>(5);
    CHECK(p);                                          // pointer conversion
    CHECK((std::vector<int>{1, 2, 3}.size() == 3));    // braces with commas inside an extra pair of parentheses
    CHECK(std::vector<int>{1, 2}.size() == 2);         // variadic macro: unparenthesised commas inside braces are re-joined
    if (true) CHECK(true); else CHECK(false);          // usable as a single statement
    vynex_test::context() = "selftest context";
    vynex_test::context().clear();
#ifdef NDEBUG
    std::printf("selftest passed (NDEBUG is defined: CHECK is active regardless)\n");
#else
    std::printf("selftest passed (NDEBUG is not defined)\n");
#endif
    return 0;
}
