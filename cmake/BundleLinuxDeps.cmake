# BundleLinuxDeps.cmake - copy the runtime shared-library closure for Linux.
# Usage (install time, via install(CODE)):
#   cmake -DEXE=<installed binary> -DLIBDIR=<install lib/> -DSEARCH_LIBDIRS="a;b" -P BundleLinuxDeps.cmake
#
# Resolves `ldd` recursively starting from EXE. Copies everything EXCEPT the
# system set that stays on the target machine: the C runtime, libstdc++/
# libgcc_s (forward compatible — newer systems satisfy older needs; bundling
# older ones risks symbol conflicts), and X11/GL/xkb/dbus display
# prerequisites (declared in the .deb instead; bundling mesa libGL would
# break vendor driver dispatch). Anything else reported "not found" is fatal:
# the package would not run.
if(NOT EXE OR NOT LIBDIR)
    message(FATAL_ERROR "BundleLinuxDeps.cmake requires EXE and LIBDIR")
endif()

set(_queue "${EXE}")
set(_seen "")
while(_queue)
    list(POP_FRONT _queue _f)
    execute_process(COMMAND ldd "${_f}" OUTPUT_VARIABLE _out RESULT_VARIABLE _rc)
    string(REPLACE "\n" ";" _lines "${_out}")
    foreach(_line IN LISTS _lines)
        # Forms: "  libfoo.so.1 => /path/libfoo.so.1 (0x...)" or "... => not found".
        # linux-vdso and ld-linux lines have no "=> /path" and fall through.
        if(_line MATCHES "^[ \t]*([^ \t]+)[ \t]+=>[ \t]+not found")
            set(_name "${CMAKE_MATCH_1}")
            set(_path "")
            foreach(_d IN LISTS SEARCH_LIBDIRS)
                if(EXISTS "${_d}/${_name}")
                    set(_path "${_d}/${_name}")
                    break()
                endif()
            endforeach()
            if(NOT _path)
                message(FATAL_ERROR "BundleLinuxDeps: ${_name} needed by ${_f} found nowhere (${SEARCH_LIBDIRS})")
            endif()
        elseif(_line MATCHES "^[ \t]*([^ \t]+)[ \t]+=>[ \t]+(/[^ \t]+)")
            set(_name "${CMAKE_MATCH_1}")
            set(_path "${CMAKE_MATCH_2}")
        else()
            continue()
        endif()
        # System set: stays on the target machine, never bundled.
        if(_name MATCHES "^(ld-linux|libc\\.so|libm\\.so|libdl\\.so|libpthread\\.so|librt\\.so|libresolv\\.so|libcrypt\\.so|libutil\\.so|libnss_(files|dns|compat|hesiod)|libX|libxcb|libxkb|libGL|libEGL|libGLES|libOpenGL|libdbus|libstdc\\+\\+\\.so|libgcc_s\\.so)")
            continue()
        endif()
        if(_name IN_LIST _seen)
            continue()
        endif()
        list(APPEND _seen "${_name}")
        if(NOT EXISTS "${_path}")
            message(FATAL_ERROR "BundleLinuxDeps: resolved path does not exist: ${_path}")
        endif()
        # Copy the real file; re-create the SONAME link when ldd reported one
        # (e.g. libfoo.so.1 => .../libfoo.so.1.2.3) so the loader finds the
        # exact name it looks up.
        get_filename_component(_real "${_path}" NAME)
        file(COPY "${_path}" DESTINATION "${LIBDIR}")
        if(NOT _real STREQUAL _name)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${_real}" "${LIBDIR}/${_name}")
        endif()
        message(STATUS "BundleLinuxDeps: ${_name} -> ${LIBDIR}")
        list(APPEND _queue "${LIBDIR}/${_real}")
    endforeach()
endwhile()
