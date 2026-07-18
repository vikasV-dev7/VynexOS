cmake_minimum_required(VERSION 3.20)
project(VynexOS VERSION 0.6.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Global output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Public includes
include_directories(${PROJECT_SOURCE_DIR}/include)

# Configuration Options
option(VYNEX_ENABLE_SDL2 "Enable physical display and input using SDL2" ON)

if(VYNEX_ENABLE_SDL2)
    # Disable building tests and extra features in SDL2
    set(SDL_TESTS OFF CACHE BOOL "Disable SDL2 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Disable SDL2 examples" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL2 static library" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "Build SDL2 shared library" FORCE)

    include(FetchContent)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.9
    )
    FetchContent_MakeAvailable(SDL2)
    add_compile_definitions(VYNEX_ENABLE_SDL2)
endif()

# Strict compilation warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
        -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wformat=2
    )
elseif(MSVC)
    add_compile_options(/W4)
endif()

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
