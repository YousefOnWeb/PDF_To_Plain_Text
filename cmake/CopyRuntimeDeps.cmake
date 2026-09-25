# CopyRuntimeDeps.cmake - generic runtime DLL deployment for MSYS2/MinGW
# Usage: cmake -D TARGET_FILE=<exe> -D DEST_DIR=<dir> -D SEARCH_DIRS="a;b;c" -P CopyRuntimeDeps.cmake
if(NOT TARGET_FILE OR NOT DEST_DIR)
    message(FATAL_ERROR "TARGET_FILE and DEST_DIR must be set")
endif()
if(NOT SEARCH_DIRS)
    set(SEARCH_DIRS "")
endif()

# Resolve all runtime dependencies (recursive) for the target executable
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${TARGET_FILE}"
    RESOLVED_DEPENDENCIES_VAR _resolved
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    DIRECTORIES ${SEARCH_DIRS}
    POST_EXCLUDE_REGEXES ".*system32.*" ".*api-ms-win.*"
    PRE_EXCLUDE_REGEXES ".*system32.*" ".*api-ms-win.*"
)

if(_unresolved)
    # Filter out api-ms-win virtual API sets (Windows 10+ Umbrella libs, not real files)
    set(_filtered "")
    foreach(_u IN LISTS _unresolved)
        if(NOT _u MATCHES "^api-ms-win-")
            list(APPEND _filtered "${_u}")
        endif()
    endforeach()
    if(_filtered)
        message(STATUS "Unresolved runtime deps (may be system): ${_filtered}")
    endif()
endif()

foreach(_dep IN LISTS _resolved)
    if(EXISTS "${_dep}")
        get_filename_component(_name "${_dep}" NAME)
        # Only copy DLLs (skip .so, .dylib)
        if(_name MATCHES "\\.dll$")
            file(COPY "${_dep}" DESTINATION "${DEST_DIR}")
            message(STATUS "CopyRuntimeDeps: ${_name} -> ${DEST_DIR}")
        endif()
    endif()
endforeach()
