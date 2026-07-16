add_library(vynex_apps STATIC demo_app.cpp desktop_shell.cpp basic_terminal.cpp basic_file_explorer.cpp)
target_include_directories(vynex_apps PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(vynex_apps PUBLIC vynex_desktop)
