add_library(vynex_core STATIC
    file_logger.cpp
    basic_plugin_manager.cpp
    plugin_context.cpp
    plugin_context_factory.cpp
    loaded_plugin.cpp
    local_ipc_framework.cpp
    basic_task_scheduler.cpp
    frame_clock.cpp
)
target_include_directories(vynex_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
