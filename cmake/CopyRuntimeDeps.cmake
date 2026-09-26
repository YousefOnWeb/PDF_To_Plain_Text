# CopyRuntimeDeps.cmake - generic runtime DLL deployment for MSYS2/MinGW
# Usage: cmake -D TARGET_FILE=<exe> -D DEST_DIR=<dir> -D SEARCH_DIRS="a;b;c" -P CopyRuntimeDeps.cmake
if(NOT TARGET_FILE OR NOT DEST_DIR)
    message(FATAL_ERROR "TARGET_FILE and DEST_DIR must be set")
endif()
if(NOT SEARCH_DIRS)
    set(SEARCH_DIRS "")
endif()

# Resolve all runtime dependencies (recursive) for the target executable.
# CONFLICTING_DEPENDENCIES_PREFIX keeps this from dying with a fatal error
# when a DLL exists both next to the exe (e.g. put there by windeployqt)
# and in a search dir — that case is handled explicitly below.
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${TARGET_FILE}"
    RESOLVED_DEPENDENCIES_VAR _resolved
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    CONFLICTING_DEPENDENCIES_PREFIX _conflict
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

# A DLL found in several places (verified: Qt6Core.dll beside the exe after
# windeployqt plus the same file in the toolchain bin dir). If DEST_DIR
# already has it, nothing to do; otherwise copy the first candidate so the
# dependency is never silently dropped.
foreach(_f IN LISTS _conflict_FILENAMES)
    if(EXISTS "${DEST_DIR}/${_f}")
        message(STATUS "CopyRuntimeDeps: ${_f} already in ${DEST_DIR}, skipping")
    else()
        list(GET _conflict_${_f} 0 _first)
        if(_first MATCHES "\\.dll$" AND EXISTS "${_first}")
            file(COPY "${_first}" DESTINATION "${DEST_DIR}")
            message(STATUS "CopyRuntimeDeps: ${_f} (conflict, took ${_first}) -> ${DEST_DIR}")
        else()
            message(WARNING "CopyRuntimeDeps: conflicting paths for ${_f}, none usable: ${_conflict_${_f}}")
        endif()
    endif()
endforeach()
