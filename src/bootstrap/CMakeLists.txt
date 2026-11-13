# Startup configuration: which filesystem profile to seed, how the command line selects it, and the scene snapshot test
# interface (--scene-snapshot). Linked by everything that compiles composition_root.cpp.
add_library(vynex_startup STATIC vfs_profiles.cpp startup_options.cpp scene_snapshot.cpp)
target_include_directories(vynex_startup PUBLIC ${PROJECT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vynex_startup PUBLIC vynex_core)

add_executable(vynex_init main.cpp composition_root.cpp)
target_link_libraries(vynex_init PRIVATE vynex_startup vynex_core vynex_desktop vynex_hal vynex_apps)
target_include_directories(vynex_init PRIVATE ${PROJECT_SOURCE_DIR}/include)
