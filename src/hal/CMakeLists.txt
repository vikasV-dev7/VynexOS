set(HAL_SOURCES
    mock_input_driver.cpp
    mock_audio_driver.cpp
    mock_compute_driver.cpp
    mock_network_adapter.cpp
    mock_block_device.cpp
    mock_hardware_clock.cpp
    library_loader_factory.cpp
)

if(VYNEX_ENABLE_SDL2)
    list(APPEND HAL_SOURCES sdl2_input_driver.cpp)
endif()

if(WIN32)
    list(APPEND HAL_SOURCES windows_library_loader.cpp)
else()
    list(APPEND HAL_SOURCES posix_library_loader.cpp)
    # dlopen requires libdl on some POSIX systems
    list(APPEND EXTRA_LIBS ${CMAKE_DL_LIBS})
endif()

add_library(vynex_hal STATIC ${HAL_SOURCES})
if(EXTRA_LIBS)
    target_link_libraries(vynex_hal PRIVATE ${EXTRA_LIBS})
endif()
target_include_directories(vynex_hal PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(vynex_hal PUBLIC vynex_core)

if(VYNEX_ENABLE_SDL2)
    target_link_libraries(vynex_hal PRIVATE SDL2-static)
endif()
