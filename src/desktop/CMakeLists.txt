set(DESKTOP_SOURCES
    mock_display_backend.cpp
    basic_compositor.cpp
    basic_window_manager.cpp
    basic_widget_toolkit.cpp
    basic_window_decorator.cpp
    basic_notification_service.cpp
    software_surface.cpp
    basic_taskbar.cpp
    dirty_region.cpp
    decoration_layout.cpp
    painter.cpp
    utf8.cpp
    font_face.cpp
    font_data.cpp
)

# Embed the Inter fonts (unmodified, hash-verified at configure time) into the executable as byte arrays.
set(VYNEX_FONT_DIR ${PROJECT_SOURCE_DIR}/assets/fonts/inter)
set(VYNEX_FONT_DATA_CPP ${CMAKE_CURRENT_BINARY_DIR}/font_data_generated.cpp)
add_custom_command(
    OUTPUT ${VYNEX_FONT_DATA_CPP}
    COMMAND vynex_bin2c ${VYNEX_FONT_DATA_CPP} font_data.hpp vynexos::desktop::fonts
            kInterRegular=${VYNEX_FONT_DIR}/Inter-Regular.ttf
            kInterMedium=${VYNEX_FONT_DIR}/Inter-Medium.ttf
            kInterSemiBold=${VYNEX_FONT_DIR}/Inter-SemiBold.ttf
    DEPENDS vynex_bin2c
            ${VYNEX_FONT_DIR}/Inter-Regular.ttf
            ${VYNEX_FONT_DIR}/Inter-Medium.ttf
            ${VYNEX_FONT_DIR}/Inter-SemiBold.ttf
    COMMENT "Embedding Inter fonts into font_data_generated.cpp"
    VERBATIM)
list(APPEND DESKTOP_SOURCES ${VYNEX_FONT_DATA_CPP})

if(VYNEX_ENABLE_SDL2)
    list(APPEND DESKTOP_SOURCES sdl2_display_backend.cpp)
endif()

add_library(vynex_desktop STATIC ${DESKTOP_SOURCES})
target_include_directories(vynex_desktop PUBLIC ${PROJECT_SOURCE_DIR}/include)
# Internal headers (font_data.hpp is included by the generated source), and the vendored third-party header as a
# SYSTEM include so its warnings are not ours.
target_include_directories(vynex_desktop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(vynex_desktop SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
target_link_libraries(vynex_desktop PUBLIC vynex_core)

if(VYNEX_ENABLE_SDL2)
    target_link_libraries(vynex_desktop PRIVATE SDL2-static)
endif()
