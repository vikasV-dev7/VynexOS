set(DESKTOP_SOURCES
    mock_display_backend.cpp
    basic_compositor.cpp
    basic_window_manager.cpp
    basic_widget_toolkit.cpp
    basic_notification_service.cpp
    software_surface.cpp
)

if(VYNEX_ENABLE_SDL2)
    list(APPEND DESKTOP_SOURCES sdl2_display_backend.cpp)
endif()

add_library(vynex_desktop STATIC ${DESKTOP_SOURCES})
target_include_directories(vynex_desktop PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(vynex_desktop PUBLIC vynex_core)

if(VYNEX_ENABLE_SDL2)
    target_link_libraries(vynex_desktop PRIVATE SDL2-static)
endif()
