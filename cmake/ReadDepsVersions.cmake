# Reads simple "KEY = VALUE" pairs from thirdParty/versions.cfg and exposes each
# as a CMake variable of the same name in the caller's scope. Lines starting with
# '#' and blank lines are ignored; inline '#' comments are stripped.
#
# Usage:
#   include(cmake/ReadDepsVersions.cmake)
#   te_read_deps_versions("${CMAKE_SOURCE_DIR}/thirdParty/versions.cfg")
#   message(STATUS "RenderDoc: ${RENDERDOC_VERSION}")

function(te_read_deps_versions CFG_PATH)
    if(NOT EXISTS "${CFG_PATH}")
        message(FATAL_ERROR "Dependency version file not found: ${CFG_PATH}")
    endif()

    file(STRINGS "${CFG_PATH}" _lines)
    foreach(_line IN LISTS _lines)
        # Strip inline comments and surrounding whitespace.
        string(REGEX REPLACE "#.*$" "" _line "${_line}")
        string(STRIP "${_line}" _line)
        if(_line STREQUAL "")
            continue()
        endif()
        if(NOT _line MATCHES "^([A-Za-z_][A-Za-z0-9_]*)[ \t]*=[ \t]*(.*)$")
            message(WARNING "versions.cfg: ignoring malformed line: ${_line}")
            continue()
        endif()
        set(_key "${CMAKE_MATCH_1}")
        set(_val "${CMAKE_MATCH_2}")
        string(STRIP "${_val}" _val)
        # PARENT_SCOPE so the caller sees it.
        set(${_key} "${_val}" PARENT_SCOPE)
    endforeach()
endfunction()
