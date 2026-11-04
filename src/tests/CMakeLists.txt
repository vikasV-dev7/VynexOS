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
    target_link_libraries(test_sdl_integration PRIVATE vynex_startup vynex_core vynex_desktop vynex_hal vynex_apps SDL2-static)
    add_test(NAME SDLIntegrationTests COMMAND test_sdl_integration)

    # Framebuffer -> texture channel order (headless: software renderer, no window)
    add_executable(test_sdl_pixel_format test_sdl_pixel_format.cpp)
    target_link_libraries(test_sdl_pixel_format PRIVATE SDL2-static)
    add_test(NAME SDLPixelFormatTests COMMAND test_sdl_pixel_format)

    # Keyboard path end to end: SDL events -> SDL2 input driver -> event bus -> shell -> focused application
    add_executable(test_keyboard_sdl_path test_keyboard_sdl_path.cpp)
    target_include_directories(test_keyboard_sdl_path PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(test_keyboard_sdl_path PRIVATE vynex_apps vynex_core vynex_desktop vynex_hal SDL2-static)
    add_test(NAME KeyboardSdlPathTests COMMAND test_keyboard_sdl_path)
endif()

add_executable(test_runtime_bug test_runtime_bug.cpp ${PROJECT_SOURCE_DIR}/src/bootstrap/composition_root.cpp)
target_include_directories(test_runtime_bug PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_runtime_bug PRIVATE vynex_startup vynex_core vynex_hal vynex_desktop vynex_apps)

add_executable(test_hal test_hal.cpp ${PROJECT_SOURCE_DIR}/src/bootstrap/composition_root.cpp)
target_include_directories(test_hal PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_hal PRIVATE vynex_startup vynex_core vynex_hal vynex_desktop vynex_apps)

add_executable(test_compositor test_compositor.cpp)
target_include_directories(test_compositor PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_compositor PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_compositor_bounds test_compositor_bounds.cpp ${PROJECT_SOURCE_DIR}/src/apps/desktop_shell.cpp)
target_include_directories(test_compositor_bounds PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_compositor_bounds PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_widget_toolkit test_widget_toolkit.cpp)
target_include_directories(test_widget_toolkit PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_widget_toolkit PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_window_manager test_window_manager.cpp)
target_include_directories(test_window_manager PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_window_manager PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_window_chrome test_window_chrome.cpp)
target_include_directories(test_window_chrome PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_window_chrome PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_chrome_interaction test_chrome_interaction.cpp ${PROJECT_SOURCE_DIR}/src/apps/desktop_shell.cpp)
target_include_directories(test_chrome_interaction PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_chrome_interaction PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_painter test_painter.cpp)
target_include_directories(test_painter PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_painter PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_theme test_theme.cpp)
target_include_directories(test_theme PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_font_glyphs test_font_glyphs.cpp)
target_include_directories(test_font_glyphs PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_font_glyphs PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_utf8 test_utf8.cpp)
target_include_directories(test_utf8 PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_utf8 PRIVATE vynex_desktop vynex_core)

add_executable(test_font_face test_font_face.cpp)
target_include_directories(test_font_face PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_font_face PRIVATE vynex_desktop vynex_core)

add_executable(test_text_renderer test_text_renderer.cpp)
target_include_directories(test_text_renderer PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_text_renderer PRIVATE vynex_desktop vynex_core)

add_executable(test_toolkit_text test_toolkit_text.cpp)
target_include_directories(test_toolkit_text PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_toolkit_text PRIVATE vynex_desktop vynex_core)

# Frame-cost benchmark: built but deliberately not registered with CTest (it reports numbers, it does not assert).
add_executable(bench_desktop_frame bench_desktop_frame.cpp)
target_include_directories(bench_desktop_frame PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(bench_desktop_frame PRIVATE vynex_apps vynex_core vynex_desktop vynex_hal)
# The --dump mode writes BMP files with plain stdio; this is a developer tool, not shipped code.
target_compile_definitions(bench_desktop_frame PRIVATE _CRT_SECURE_NO_WARNINGS)

add_executable(test_input_display test_input_display.cpp ${PROJECT_SOURCE_DIR}/src/apps/desktop_shell.cpp)
target_include_directories(test_input_display PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_input_display PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_desktop_shell test_desktop_shell.cpp ${PROJECT_SOURCE_DIR}/src/apps/desktop_shell.cpp)
target_include_directories(test_desktop_shell PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_desktop_shell PRIVATE vynex_core vynex_desktop vynex_hal)

add_executable(test_virtual_path test_virtual_path.cpp)
target_include_directories(test_virtual_path PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_virtual_path PRIVATE vynex_core)

add_executable(test_vnode test_vnode.cpp)
target_include_directories(test_vnode PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_vnode PRIVATE vynex_core)

add_executable(test_memory_file_handle test_memory_file_handle.cpp)
target_include_directories(test_memory_file_handle PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_memory_file_handle PRIVATE vynex_core)

add_executable(test_memory_file_system test_memory_file_system.cpp)
target_include_directories(test_memory_file_system PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_memory_file_system PRIVATE vynex_core)

add_executable(test_application_registry test_application_registry.cpp)
target_include_directories(test_application_registry PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_application_registry PRIVATE vynex_apps vynex_core)

add_executable(test_application_integration test_application_integration.cpp)
target_include_directories(test_application_integration PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_application_integration PRIVATE vynex_apps vynex_core vynex_desktop vynex_hal)

add_executable(test_basic_file_explorer test_basic_file_explorer.cpp ${PROJECT_SOURCE_DIR}/src/apps/basic_file_explorer.cpp)
target_include_directories(test_basic_file_explorer PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_basic_file_explorer PRIVATE vynex_apps vynex_core vynex_desktop vynex_hal)

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
add_test(NAME CompositorBoundsTests COMMAND test_compositor_bounds)
set_tests_properties(CompositorBoundsTests PROPERTIES TIMEOUT 300)
add_test(NAME WidgetToolkitTests COMMAND test_widget_toolkit)
add_test(NAME WindowManagerTests COMMAND test_window_manager)
add_test(NAME DesktopShellTests COMMAND test_desktop_shell)
add_test(NAME InputDisplayTests COMMAND test_input_display)
add_test(NAME FontGlyphTests COMMAND test_font_glyphs)
add_test(NAME WindowChromeTests COMMAND test_window_chrome)
add_test(NAME ChromeInteractionTests COMMAND test_chrome_interaction)
add_test(NAME PainterTests COMMAND test_painter)
add_test(NAME ThemeTests COMMAND test_theme)
add_test(NAME Utf8Tests COMMAND test_utf8)
add_test(NAME FontFaceTests COMMAND test_font_face ${PROJECT_SOURCE_DIR}/assets/fonts/inter)
add_test(NAME TextRendererTests COMMAND test_text_renderer)
add_test(NAME ToolkitTextTests COMMAND test_toolkit_text)
add_test(NAME VirtualPathTests COMMAND test_virtual_path)
add_test(NAME VNodeTests COMMAND test_vnode)
add_test(NAME MemoryFileHandleTests COMMAND test_memory_file_handle)
add_test(NAME MemoryFileSystemTests COMMAND test_memory_file_system)
add_test(NAME ApplicationRegistryTests COMMAND test_application_registry)
add_test(NAME ApplicationIntegrationTests COMMAND test_application_integration)
add_test(NAME BasicFileExplorerTests COMMAND test_basic_file_explorer)

# Application content reaching the screen: File Explorer hosted by the real shell, window manager and compositor.
add_executable(test_explorer_content_presentation test_explorer_content_presentation.cpp)
target_include_directories(test_explorer_content_presentation PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_explorer_content_presentation PRIVATE vynex_apps vynex_core vynex_desktop vynex_hal)
add_test(NAME ExplorerContentPresentationTests COMMAND test_explorer_content_presentation)

# The shell's side of IApplication::take_invalidated_region (scripted applications).
add_executable(test_app_invalidation test_app_invalidation.cpp)
target_include_directories(test_app_invalidation PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_app_invalidation PRIVATE vynex_apps vynex_core vynex_desktop vynex_hal)
add_test(NAME AppInvalidationTests COMMAND test_app_invalidation)

# Geometry contract: application pointer coordinates and surfaces follow the window's client area
add_executable(test_app_geometry test_app_geometry.cpp)
target_include_directories(test_app_geometry PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_app_geometry PRIVATE vynex_apps vynex_core vynex_desktop vynex_hal)
add_test(NAME AppGeometryTests COMMAND test_app_geometry)

# Application lifecycle: launcher, application host, close/reopen, keyboard routing, Terminal
add_executable(test_app_lifecycle test_app_lifecycle.cpp)
target_include_directories(test_app_lifecycle PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_app_lifecycle PRIVATE vynex_apps vynex_core vynex_desktop vynex_hal)
add_test(NAME AppLifecycleTests COMMAND test_app_lifecycle)

# Initial filesystem contents: the generic seeding helper (vynex_core) and the startup profiles and options (vynex_startup)
add_executable(test_vfs_seed test_vfs_seed.cpp)
target_include_directories(test_vfs_seed PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_vfs_seed PRIVATE vynex_core)
add_test(NAME VfsSeedTests COMMAND test_vfs_seed)

add_executable(test_startup_profiles test_startup_profiles.cpp)
target_link_libraries(test_startup_profiles PRIVATE vynex_startup vynex_core)
add_test(NAME StartupProfileTests COMMAND test_startup_profiles)

add_executable(test_composition_vfs test_composition_vfs.cpp ${PROJECT_SOURCE_DIR}/src/bootstrap/composition_root.cpp)
target_include_directories(test_composition_vfs PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_composition_vfs PRIVATE vynex_startup vynex_core vynex_hal vynex_desktop vynex_apps)
add_test(NAME CompositionRootVfsTests COMMAND test_composition_vfs)
# The program itself refuses an invalid command line (exit status 2, a diagnostic, no desktop started).
add_test(NAME StartupRejectsBadOptions COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:vynex_init> -P ${PROJECT_SOURCE_DIR}/cmake/check_startup_rejects.cmake)

# --- Test mechanism: self-tests and source hygiene ------------------------------------------------------------------------
# CHECK() (test_check.hpp) must fail on a false condition and evaluate its argument in every configuration, NDEBUG included.
# The two *Detects* tests pass only when the process exits non-zero (WILL_FAIL), so a CHECK that stopped working would fail them.
add_executable(test_check_selftest test_check_selftest.cpp)
add_test(NAME CheckMechanismPasses COMMAND test_check_selftest --pass)
add_test(NAME CheckMechanismDetectsFalseCondition COMMAND test_check_selftest --fail)
add_test(NAME CheckMechanismDetectsFalseAfterSideEffect COMMAND test_check_selftest --fail-side-effect)
set_tests_properties(CheckMechanismDetectsFalseCondition CheckMechanismDetectsFalseAfterSideEffect PROPERTIES WILL_FAIL TRUE)
# Test sources must not use assert() (it vanishes under NDEBUG).
add_test(NAME TestSourcesHaveNoAssert COMMAND ${CMAKE_COMMAND} -DTEST_DIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${PROJECT_SOURCE_DIR}/cmake/check_no_assert.cmake)

# --- Timeouts -------------------------------------------------------------------------------------------------------------
# Every test has an explicit timeout so a hung or crash-stalled test fails instead of blocking CTest (its default is 25 min).
# Slower tests are listed here (seconds); every other registered test gets the 120 s default below. Measured: the slowest are
# SDLIntegrationTests (~50 s, real window) and ChromeInteractionTests (~37 s in Debug).
if(TEST SDLIntegrationTests)
    set_tests_properties(SDLIntegrationTests PROPERTIES TIMEOUT 300)
endif()
set_tests_properties(ChromeInteractionTests PROPERTIES TIMEOUT 180)
get_directory_property(_vynex_tests TESTS)
if(NOT _vynex_tests)
    message(FATAL_ERROR "cannot enumerate the registered tests to assign timeouts")
endif()
foreach(_vynex_test IN LISTS _vynex_tests)
    get_test_property(${_vynex_test} TIMEOUT _vynex_timeout)
    if(NOT _vynex_timeout)
        set_tests_properties(${_vynex_test} PROPERTIES TIMEOUT 120)
    endif()
endforeach()
