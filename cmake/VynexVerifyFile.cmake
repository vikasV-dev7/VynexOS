# vynex_verify_sha256(<path> <expected-sha256> <description>)
#
# Fails the CMake run when a vendored third-party file is missing or differs from the audited, pinned version.
# Kept in its own file (no side effects) so it can be unit-tested with `cmake -P`.
function(vynex_verify_sha256 path expected description)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Vendored file is missing: ${description}\n  ${path}")
    endif()
    file(SHA256 "${path}" actual)
    string(TOLOWER "${expected}" expected_lower)
    if(NOT actual STREQUAL expected_lower)
        message(FATAL_ERROR
            "Vendored file does not match its audited checksum: ${description}\n"
            "  file:     ${path}\n"
            "  expected: ${expected_lower}\n"
            "  actual:   ${actual}\n"
            "These files must stay byte-identical to the upstream copies (see the README next to them).")
    endif()
endfunction()
