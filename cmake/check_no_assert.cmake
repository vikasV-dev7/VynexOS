# Fails if a test source uses assert(...): <cassert>'s assert() compiles to nothing under NDEBUG, so in Release a test would
# lose its checks and any call placed inside them. Test checks use CHECK() from test_check.hpp, which is always active.
# Run by CTest as:  cmake -DTEST_DIR=<src/tests> -P cmake/check_no_assert.cmake
cmake_minimum_required(VERSION 3.20)

if(NOT TEST_DIR)
    message(FATAL_ERROR "TEST_DIR is not set")
endif()

file(GLOB _sources "${TEST_DIR}/test_*.cpp")
list(LENGTH _sources _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "no test sources found in ${TEST_DIR}")
endif()

set(_offenders "")
foreach(_file IN LISTS _sources)
    file(READ "${_file}" _text)
    # `assert(` that is not part of a longer identifier (static_assert, my_assert, ...).
    string(REGEX MATCH "(^|[^A-Za-z0-9_])assert[ \t]*\\(" _hit "${_text}")
    if(_hit)
        get_filename_component(_name "${_file}" NAME)
        list(APPEND _offenders "${_name}")
    endif()
endforeach()

if(_offenders)
    list(JOIN _offenders ", " _joined)
    message(FATAL_ERROR "assert() found in test sources (it disappears under NDEBUG; use CHECK() from test_check.hpp): ${_joined}")
endif()
message(STATUS "TestSourcesHaveNoAssert: scanned ${_count} test sources, no assert() found")
