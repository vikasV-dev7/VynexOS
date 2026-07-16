add_library(vynex_hal STATIC    mock_input_driver.cpp
    mock_audio_driver.cpp
    mock_compute_driver.cpp
)
target_include_directories(vynex_hal PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(vynex_hal PUBLIC vynex_core)
