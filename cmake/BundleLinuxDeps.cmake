# BundleLinuxDeps.cmake - copy the runtime shared-library closure for Linux.
# Usage (install time, via install(CODE)):
#   cmake -DEXE=<installed binary> -DLIBDIR=<install lib/> -DSEARCH_LIST_FILE=<file> -P BundleLinuxDeps.cmake
# where <file> holds one search dir per line. Do NOT pass the list itself via
# -D: a semicolon list cannot survive install(CODE "...") intact in any
# quoting style, so -DSEARCH_LIBDIRS is only a fallback for direct invocation.
#
# NOTE ON STYLE: this file runs via `cmake -P`, which sets no policies
# (there is no project() call in script mode), so cmake_minimum_required
# below is load-bearing, not decorative: without it, IN_LIST is a syntax
# error (CMP0057) and empty list elements warn (CMP0007). Likewise, regexes
# use bracket classes like [.] instead of backslash escapes: backslash
# handling differs between layers here and has caused real bugs before.
cmake_minimum_required(VERSION 3.21)
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
# Search dirs arrive via file, not -D: a semicolon list cannot survive
# install(CODE "...") intact (bare quotes terminate the outer string, and
# embedded quotes do not group, so the value splits and corrupts).
if(DEFINED SEARCH_LIST_FILE AND EXISTS "${SEARCH_LIST_FILE}")
    file(READ "${SEARCH_LIST_FILE}" _search_raw)
    string(REPLACE "\n" ";" SEARCH_LIBDIRS "${_search_raw}")
endif()
if(NOT SEARCH_LIBDIRS)
    message(STATUS "BundleLinuxDeps: no search dirs; only absolute ldd paths will resolve")
endif()
# Self-report: install(CODE) diagnostics have proven unreliable witnesses in
# CI logs, so the script states its own inputs. If SEARCH_LIBDIRS ever prints
# short here, the problem is upstream (generation), not in this script.
message(STATUS "BundleLinuxDeps: EXE=${EXE}")
message(STATUS "BundleLinuxDeps: LIBDIR=${LIBDIR}")
message(STATUS "BundleLinuxDeps: SEARCH_LIBDIRS=${SEARCH_LIBDIRS}")

set(_queue "${EXE}")
set(_seen "")
while(_queue)
    list(POP_FRONT _queue _f)
    execute_process(COMMAND ldd "${_f}" OUTPUT_VARIABLE _out RESULT_VARIABLE _rc)
    # Loud by design: an ldd failure or an empty/unparseable listing must abort
    # here. The alternative observed in the wild is a silently empty lib/
    # directory and a package that fails only later at launch.
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "BundleLinuxDeps: ldd failed on ${_f} (exit ${_rc})")
    endif()
    string(REPLACE "\n" ";" _lines "${_out}")
    list(LENGTH _lines _n)
    message(STATUS "BundleLinuxDeps: ldd reports ${_n} lines for ${_f}")
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
                # Resolve a missing SONAME against versioned files, e.g.
                # libpoppler-qt6.so.3 against libpoppler-qt6.so.3.0.0 when the
                # symlink itself is absent from the search dir.
                if(NOT _path AND _name MATCHES "^(.*[.]so[.][0-9]+)$")
                    file(GLOB _candidates "${_d}/${CMAKE_MATCH_1}.*")
                    list(SORT _candidates)
                    if(_candidates)
                        list(GET _candidates 0 _path)
                        break()
                    endif()
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
        # (Bracket classes like [.] are used instead of backslash escapes:
        # backslash handling differs between layers here and has caused real
        # bugs before. See the header note on cmake_minimum_required.)
        # Names containing a slash (e.g. lib64/ld-linux-x86-64.so.2 as the
        # left-hand token on some distros) are loader paths, never SONAMEs.
        if(_name MATCHES "/")
            continue()
        endif()
        if(_name MATCHES "^(ld-linux|libc[.]so|libm[.]so|libdl[.]so|libpthread[.]so|librt[.]so|libresolv[.]so|libcrypt[.]so|libutil[.]so|libnss_(files|dns|compat|hesiod)|libX|libxcb|libxkb|libGL|libEGL|libGLES|libOpenGL|libdbus|libstdc[+][+]so|libgcc_s[.]so)")
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
        # NOTE: _path itself may be a symlink (Arch: /usr/lib/libQt6*.so.6 ->
        # libQt6*.so.6.x.y). file(COPY) preserves symlinks, so copying _path
        # directly would stage a dangling link and the later recursive ldd on
        # it fails with "No such file or directory". Resolve first, then link.
        get_filename_component(_resolved "${_path}" REALPATH)
        get_filename_component(_real "${_resolved}" NAME)
        file(COPY "${_resolved}" DESTINATION "${LIBDIR}")
        if(NOT _real STREQUAL _name)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${_real}" "${LIBDIR}/${_name}")
        endif()
        message(STATUS "BundleLinuxDeps: ${_name} -> ${LIBDIR}")
        list(APPEND _queue "${LIBDIR}/${_real}")
    endforeach()
endwhile()
list(LENGTH _seen _total)
message(STATUS "BundleLinuxDeps: staged ${_total} libraries in ${LIBDIR}")
