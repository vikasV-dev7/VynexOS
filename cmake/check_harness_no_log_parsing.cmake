# Fails if the UI harness reads window state from the runtime log. Window positions, states and scene membership come
# from the app's scene snapshot file (--scene-snapshot, src/bootstrap/scene_snapshot.hpp); the log is free to change
# format, and the app no longer writes a periodic scene dump for anything to parse.
# Run by CTest as:  cmake -DHARNESS_DIR=<tools/ui_harness> -P cmake/check_harness_no_log_parsing.cmake
cmake_minimum_required(VERSION 3.20)

if(NOT HARNESS_DIR)
    message(FATAL_ERROR "HARNESS_DIR is not set")
endif()

file(GLOB _scripts "${HARNESS_DIR}/*.ps1")
list(LENGTH _scripts _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "no harness scripts found in ${HARNESS_DIR}")
endif()

# Markers of the removed periodic scene dump: a script matching any of them is parsing window state out of the log.
set(_markers "Generated SceneGraph" "RenderLayer Content" "RenderLayer count" "Excluded Window ID" "Registered Windows")
set(_offenders "")
set(_reads_snapshot FALSE)
foreach(_file IN LISTS _scripts)
    file(READ "${_file}" _text)
    get_filename_component(_name "${_file}" NAME)
    foreach(_marker IN LISTS _markers)
        string(FIND "${_text}" "${_marker}" _at)
        if(NOT _at EQUAL -1)
            list(APPEND _offenders "${_name}: '${_marker}'")
        endif()
    endforeach()
    string(FIND "${_text}" "--scene-snapshot=" _at)
    if(NOT _at EQUAL -1)
        set(_reads_snapshot TRUE)
    endif()
endforeach()

if(_offenders)
    list(JOIN _offenders ", " _joined)
    message(FATAL_ERROR "the UI harness parses window state from the runtime log: ${_joined}")
endif()
if(NOT _reads_snapshot)
    message(FATAL_ERROR "the UI harness does not start the app with --scene-snapshot")
endif()
message(STATUS "HarnessUsesSceneSnapshot: scanned ${_count} harness scripts; window state comes from the scene snapshot")
