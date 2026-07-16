add_executable(vynex_init main.cpp composition_root.cpp)
target_link_libraries(vynex_init PRIVATE vynex_core vynex_desktop vynex_hal vynex_apps)
target_include_directories(vynex_init PRIVATE ${PROJECT_SOURCE_DIR}/include)
