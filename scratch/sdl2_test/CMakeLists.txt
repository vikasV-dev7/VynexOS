cmake_minimum_required(VERSION 3.25)
project(SDL2Test)

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
