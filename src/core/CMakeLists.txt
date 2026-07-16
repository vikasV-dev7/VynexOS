add_library(vynex_core STATIC file_logger.cpp    local_ipc_framework.cpp
    basic_plugin_manager.cpp
)
target_include_directories(vynex_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
