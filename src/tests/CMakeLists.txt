set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_executable(vynex_test_scheduler test_task_scheduler.cpp)
target_link_libraries(vynex_test_scheduler PRIVATE vynex_core)

add_library(test_plugin MODULE test_plugin.c)
target_compile_definitions(test_plugin PRIVATE VYNEX_BUILD_PLUGIN)
target_include_directories(test_plugin PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(test_plugin PROPERTIES PREFIX "")

add_library(test_plugin_invalid_abi MODULE test_plugin_invalid_abi.c)
target_compile_definitions(test_plugin_invalid_abi PRIVATE VYNEX_BUILD_PLUGIN)
target_include_directories(test_plugin_invalid_abi PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(test_plugin_invalid_abi PROPERTIES PREFIX "")

add_library(test_plugin_v2 MODULE test_plugin_v2.c)
target_compile_definitions(test_plugin_v2 PRIVATE VYNEX_BUILD_PLUGIN)
target_include_directories(test_plugin_v2 PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(test_plugin_v2 PROPERTIES PREFIX "")

add_library(test_plugin_init_fail MODULE test_plugin_init_fail.c)
target_compile_definitions(test_plugin_init_fail PRIVATE VYNEX_BUILD_PLUGIN)
target_include_directories(test_plugin_init_fail PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(test_plugin_init_fail PROPERTIES PREFIX "")

add_executable(test_plugin_manager test_plugin_manager.cpp)
target_include_directories(test_plugin_manager PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_plugin_manager PRIVATE vynex_core vynex_hal)

# SDL Integration test
if(TARGET SDL2-static)
    add_executable(test_sdl_integration test_sdl_integration.cpp ${PROJECT_SOURCE_DIR}/src/bootstrap/composition_root.cpp)
    target_link_libraries(test_sdl_integration PRIVATE vynex_core vynex_desktop vynex_hal vynex_apps SDL2-static)
    add_test(NAME SDLIntegrationTests COMMAND test_sdl_integration)
endif()

add_executable(test_runtime_bug test_runtime_bug.cpp ${PROJECT_SOURCE_DIR}/src/bootstrap/composition_root.cpp)
target_include_directories(test_runtime_bug PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_runtime_bug PRIVATE vynex_core vynex_hal vynex_desktop vynex_apps)

add_executable(test_hal test_hal.cpp ${PROJECT_SOURCE_DIR}/src/bootstrap/composition_root.cpp)
target_include_directories(test_hal PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_hal PRIVATE vynex_core vynex_hal vynex_desktop vynex_apps)

add_executable(test_compositor test_compositor.cpp)
target_include_directories(test_compositor PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_compositor PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_widget_toolkit test_widget_toolkit.cpp)
target_include_directories(test_widget_toolkit PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_widget_toolkit PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_window_manager test_window_manager.cpp)
target_include_directories(test_window_manager PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_window_manager PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_desktop_shell test_desktop_shell.cpp ${PROJECT_SOURCE_DIR}/src/apps/desktop_shell.cpp)
target_include_directories(test_desktop_shell PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_desktop_shell PRIVATE vynex_core vynex_desktop vynex_hal)

# Copy the libraries to the executable directory so it can find them
add_custom_command(TARGET test_plugin_manager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:test_plugin>
    $<TARGET_FILE:test_plugin_invalid_abi>
    $<TARGET_FILE:test_plugin_v2>
    $<TARGET_FILE:test_plugin_init_fail>
    $<TARGET_FILE_DIR:test_plugin_manager>
)

enable_testing()
add_test(NAME SchedulerTests COMMAND vynex_test_scheduler)
add_test(NAME PluginManagerTests COMMAND test_plugin_manager)
add_test(NAME HALTests COMMAND test_hal)
add_test(NAME CompositorTests COMMAND test_compositor)
add_test(NAME WidgetToolkitTests COMMAND test_widget_toolkit)
add_test(NAME WindowManagerTests COMMAND test_window_manager)
add_test(NAME DesktopShellTests COMMAND test_desktop_shell)
