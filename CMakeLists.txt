cmake_minimum_required(VERSION 3.25)
project(VynexOS VERSION 0.4.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

# Strict compilation warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wformat=2
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX)
endif()

# Global output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Public includes
include_directories(${PROJECT_SOURCE_DIR}/include)

# Subdirectories
add_subdirectory(src/hal)
add_subdirectory(src/core)
add_subdirectory(src/bootstrap)
add_subdirectory(src/ai)
add_subdirectory(src/desktop)
add_subdirectory(src/apps)
add_subdirectory(src/sdk)
add_subdirectory(src/plugins)
enable_testing()
add_subdirectory(src/tests)
