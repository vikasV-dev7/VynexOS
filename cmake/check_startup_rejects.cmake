# The vynex_init program must refuse an invalid command line: exit status 2, a one-line diagnostic on stderr, and no
# system started (a wrongly accepted command line would open the desktop window, which the TIMEOUT below then fails).
# Run by CTest as:  cmake -DEXE=<vynex_init> -P cmake/check_startup_rejects.cmake
cmake_minimum_required(VERSION 3.20)

if(NOT EXE)
    message(FATAL_ERROR "EXE is not set")
endif()

foreach(_arg IN ITEMS "--vfs-profile=bogus" "--vfs-profile=" "--vfs-profile" "--nope")
    execute_process(COMMAND "${EXE}" "${_arg}"
        RESULT_VARIABLE _status OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 20)
    if(NOT "${_status}" STREQUAL "2")
        message(FATAL_ERROR "'${_arg}': expected exit status 2, got '${_status}'\nstdout: ${_out}\nstderr: ${_err}")
    endif()
    if(NOT "${_err}" MATCHES "^vynex_init: ")
        message(FATAL_ERROR "'${_arg}': no diagnostic on stderr: '${_err}'")
    endif()
    if(NOT "${_err}" MATCHES "usage: vynex_init")
        message(FATAL_ERROR "'${_arg}': no usage line on stderr: '${_err}'")
    endif()
endforeach()

# The unknown profile name is named in the diagnostic.
execute_process(COMMAND "${EXE}" "--vfs-profile=bogus" RESULT_VARIABLE _status ERROR_VARIABLE _err TIMEOUT 20)
if(NOT "${_err}" MATCHES "unknown VFS profile 'bogus'")
    message(FATAL_ERROR "the diagnostic does not name the unknown profile: '${_err}'")
endif()
message(STATUS "vynex_init rejects invalid command lines")
