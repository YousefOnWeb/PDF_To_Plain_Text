cmake_minimum_required(VERSION 3.21)

# Single source of truth for the version. CPack derives installer filenames
# from this, so a release tag must be v<this value> (CI enforces it).
project(PDFToPlainText VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Needed on Windows for the generated .rc (icon + VERSIONINFO)
if(WIN32)
    enable_language(RC)
endif()

# --- Dependencies -------------------------------------------------------------

# Qt6 is required. Provide Qt6_DIR or CMAKE_PREFIX_PATH if Qt is in a custom location.
# Example (MinGW): -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/mingw_64"
# Example (MSYS2):  -DCMAKE_PREFIX_PATH="C:/msys64/mingw64"
find_package(Qt6 6.2 COMPONENTS Widgets Concurrent REQUIRED)

# Windows: ensure Qt DLLs are on PATH at configure time so AUTOMOC's moc.exe test runs without manual PATH setup
if(WIN32)
    if(Qt6_DIR AND EXISTS "${Qt6_DIR}/../../../bin")
        get_filename_component(_qt_bin "${Qt6_DIR}/../../../bin" ABSOLUTE)
        if(EXISTS "${_qt_bin}")
            set(ENV{PATH} "${_qt_bin};$ENV{PATH}")
        endif()
    endif()
    foreach(_p IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_p}/bin")
            set(ENV{PATH} "${_p}/bin;$ENV{PATH}")
        endif()
    endforeach()
    # Note: AUTOMOC (moc.exe) at build time still needs Qt bin on shell PATH.
    # build.ps1 handles this automatically. For manual PowerShell, run:
    #   $env:PATH = "C:/tools/msys64/ucrt64/bin;$env:PATH"
    # before cmake, or use the MSYS2 MINGW64 shell where PATH is pre-set.
endif()

# Poppler-Qt6: try Config package first (vcpkg), then pkg-config fallback (Linux/macOS system)
find_package(Poppler QUIET COMPONENTS Qt6)
if(NOT Poppler_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PopplerQt6 QUIET poppler-qt6)
    endif()
endif()

if(NOT Poppler_FOUND AND NOT PopplerQt6_FOUND)
    message(WARNING
        "Poppler-Qt6 not found. Install it before building:\n"
        "  vcpkg (MinGW): vcpkg install poppler[qt6]:x64-mingw-dynamic\n"
        "  MSYS2:   pacman -S mingw-w64-x86_64-poppler-qt6\n"
        "  Ubuntu:  sudo apt install libpoppler-qt6-dev\n"
        "  macOS:   brew install poppler\n"
        "Then re-configure with -DCMAKE_TOOLCHAIN_FILE or -DCMAKE_PREFIX_PATH pointing to the install.")
endif()

# --- Sources ------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/DropFrame.cpp
    src/DropFrame.h
    src/PdfExtractor.cpp
    src/PdfExtractor.h
    src/FileRowWidget.cpp
    src/FileRowWidget.h
    assets/app.qrc
)

add_executable(${PROJECT_NAME} ${SOURCES})

# --- Application icon ---------------------------------------------------------
set(APP_ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.png")
set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/PDFToPlainText.ico")
set(APP_ICON_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.icns")

# Windows: GUI subsystem (no console) + .rc carrying the icon and VERSIONINFO.
#
# We generate our own .rc rather than using CMake's RC_ICONS: Qt links
# libQt6EntryPoint.a, which already defines IDI_ICON1 with Qt's default
# icon. Windows only shows the *first* IDI_ICON1 group in the resource
# section, so ours has to be linked from an object file (which precede the
# archive on the link line) to win.
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

    # numeric FILEVERSION from the same project(VERSION) as the installers
    string(REPLACE "." ";" _ver "${PROJECT_VERSION}")
    list(LENGTH _ver _ver_len)
    set(_fver "${PROJECT_VERSION}")
    while(_ver_len LESS 4)
        string(APPEND _fver ".0")
        math(EXPR _ver_len "${_ver_len}+1")
    endwhile()
    string(REPLACE "." "," _fver_csv "${_fver}")

    set(_rc "${CMAKE_CURRENT_BINARY_DIR}/app_resources.rc")
    # Numeric values instead of VOS_NT_WINDOWS32 / VFT_APP: windres does not
    # pull in winver.h, so the SDK symbols are undefined at compile time.
    file(WRITE "${_rc}"
"IDI_ICON1 ICON \"${APP_ICON_ICO}\"

VS_VERSION_INFO VERSIONINFO
 FILEVERSION ${_fver_csv}
 PRODUCTVERSION ${_fver_csv}
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS 0x40004L
 FILETYPE 0x1L
 BEGIN
    BLOCK \"StringFileInfo\"
    BEGIN
        BLOCK \"040904b0\"
        BEGIN
            VALUE \"FileDescription\", \"PDF To Plain Text\"
            VALUE \"FileVersion\", \"${PROJECT_VERSION}\"
            VALUE \"ProductName\", \"PDF To Plain Text\"
            VALUE \"ProductVersion\", \"${PROJECT_VERSION}\"
        END
    END
    BLOCK \"VarFileInfo\"
    BEGIN
        VALUE \"Translation\", 0x409, 1200
    END
 END
")
    target_sources(${PROJECT_NAME} PRIVATE "${_rc}")
endif()

# macOS: build a real .app bundle so CPack DragNDrop produces a .dmg
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE "${APP_ICON_ICNS}"
        MACOSX_BUNDLE_BUNDLE_NAME "PDF To Plain Text"
        MACOSX_BUNDLE_GUI_IDENTIFIER "local.pdf-to-plain-text.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH FALSE
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Concurrent)

# Keep the in-app version string in sync with project(VERSION) above, so the
# About/version dialog can never drift from the packaged installer filenames.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PDFTOTEXT_VERSION="${PROJECT_VERSION}"
)

if(Poppler_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Poppler::Qt6)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_POPPLER_QT6_CONFIG)
elseif(PopplerQt6_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PopplerQt6_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${PopplerQt6_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PopplerQt6_LIBRARIES})
    target_compile_options(${PROJECT_NAME} PRIVATE ${PopplerQt6_CFLAGS_OTHER})
endif()

# Let Qt process the target: on Windows this is what promotes the .ico in
# assets/app.qrc to the application icon (IDI_ICON1) and links its .rc.
qt_finalize_executable(${PROJECT_NAME})

# --- Install & Deploy ---------------------------------------------------------
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)

# Linux: self-contained install tree (bundled Qt/Poppler, system X11/GL only).
# The binary finds its bundled libs via this rpath in the install tree; the
# build tree keeps its default rpath so local `./build/PDFToPlainText` runs
# keep working without env vars (BUILD_WITH_INSTALL_RPATH stays FALSE).
if(UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    # qt.conf next to the exe redirects Qt into the install tree ([Paths]
    # Prefix=.. makes Libraries=lib and Plugins=plugins resolve under root).
    # A file beside the executable overrides Qt's compiled-in paths.
    # NOTE: written as qt.conf.install, NOT qt.conf: a file named qt.conf
    # directly in CMAKE_BINARY_DIR would sit beside the build-tree exe
    # (single-config generators place it at <build>/PDFToPlainText) and
    # hijack its plugin lookup (Prefix=.. -> repo root, no plugins/),
    # aborting local runs with "no Qt platform plugin could be initialized".
    file(WRITE "${CMAKE_BINARY_DIR}/qt.conf.install" "[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\n")
    install(FILES "${CMAKE_BINARY_DIR}/qt.conf.install" DESTINATION bin RENAME qt.conf)
    # Ship the Qt plugins the GUI needs (platforms/ carries xcb, offscreen
    # and minimal; xcbglintegrations covers GL-on-X11). The Qt root is found
    # by PROBING, not by assuming Qt6_DIR's depth: depending on how Qt was
    # discovered, Qt6_DIR can be <root>/lib/cmake/Qt6, <root>/lib/cmake, or
    # <root> itself, and a wrong fixed-depth guess silently skips every rule
    # below via its EXISTS guards (observed in CI: Qt never entered the
    # search list). The first candidate actually containing the Qt6Core
    # library wins; both values are printed so the configure log settles
    # any future doubt about where Qt came from.
    set(_qt_root "")
    foreach(_c IN ITEMS "${Qt6_DIR}/../../.." "${Qt6_DIR}/../.." "${Qt6_DIR}" "$ENV{QT_ROOT_DIR}")
        get_filename_component(_cand "${_c}" ABSOLUTE)
        if(EXISTS "${_cand}/lib/libQt6Core.so.6" OR EXISTS "${_cand}/lib/libQt6Core.so")
            set(_qt_root "${_cand}")
            break()
        endif()
    endforeach()
    message(STATUS "Linux Qt bundling: Qt6_DIR=${Qt6_DIR} -> Qt root=${_qt_root}")
    if(NOT _qt_root)
        message(WARNING "Linux Qt bundling: Qt root not found; plugins and Qt search dir will be skipped")
    endif()
    foreach(_plugindir IN ITEMS platforms xcbglintegrations imageformats iconengines)
        # Standard Qt layout (<root>/plugins/...) first, then distro splits:
        # Arch/CachyOS ships them under <root>/lib/qt6/plugins/...
        foreach(_plugbase IN ITEMS "${_qt_root}/plugins" "${_qt_root}/lib/qt6/plugins" "${_qt_root}/lib/x86_64-linux-gnu/qt6/plugins")
            if(EXISTS "${_plugbase}/${_plugindir}")
                install(DIRECTORY "${_plugbase}/${_plugindir}" DESTINATION plugins)
                break()
            endif()
        endforeach()
    endforeach()
    # Bundle the runtime closure into <prefix>/lib at install time (see script
    # for the system set that deliberately stays out: C runtime, libstdc++,
    # X11/GL/xkb/dbus). Both lib/ and Debian multiarch lib/x86_64-linux-gnu
    # are searched below: GNUInstallDirs puts poppler's libraries in the
    # latter on Ubuntu.
    # The search list travels via a FILE, not a -D argument, and the CODE
    # block contains no bare quotes — both learned the hard way: in CMake
    # language embedded quotes do not group, so -DSEARCH="a;b" splits at
    # runtime (single quoted entry, rest swallowed), while bare quotes inside
    # install(CODE "...") terminate the outer string and truncate generation
    # (proven locally: execute_process body vanished from cmake_install.cmake
    # leaving an unbalanced file that fails with 'missing ending )' at install).
    set(_LINUX_SEARCH "")
    foreach(_p IN LISTS CMAKE_PREFIX_PATH)
        foreach(_subdir IN ITEMS lib lib/x86_64-linux-gnu)
            if(EXISTS "${_p}/${_subdir}")
                list(APPEND _LINUX_SEARCH "${_p}/${_subdir}")
            endif()
        endforeach()
    endforeach()
    if(EXISTS "${_qt_root}/lib")
        list(APPEND _LINUX_SEARCH "${_qt_root}/lib")
    endif()
    string(REPLACE ";" "\n" _search_nl "${_LINUX_SEARCH}")
    file(WRITE "${CMAKE_BINARY_DIR}/linux_search_dirs.txt" "${_search_nl}\n")
    install(CODE "
        set(_bundle_root \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
        set(_bundle_exe \"\${_bundle_root}/bin/${PROJECT_NAME}\")
        set(_bundle_lib \"\${_bundle_root}/lib\")
        message(STATUS \"Linux dependency search list file: ${CMAKE_BINARY_DIR}/linux_search_dirs.txt\")
        if(NOT EXISTS \"\${_bundle_exe}\")
            message(FATAL_ERROR \"Linux dependency bundling: installed executable not found: \${_bundle_exe}\")
        endif()
        execute_process(
            COMMAND \"\${CMAKE_COMMAND}\"
                -DEXE=\${_bundle_exe}
                -DLIBDIR=\${_bundle_lib}
                -DSEARCH_LIST_FILE=${CMAKE_BINARY_DIR}/linux_search_dirs.txt
                -P \"${CMAKE_SOURCE_DIR}/cmake/BundleLinuxDeps.cmake\"
            RESULT_VARIABLE _bundle_rc
        )
        if(NOT _bundle_rc EQUAL 0)
            message(FATAL_ERROR \"Linux dependency bundling failed with exit code \${_bundle_rc}\")
        endif()
    ")
endif()

# Windows: run windeployqt automatically on install + make build dir runnable without manual PATH
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6_DIR}/../../../bin" "${CMAKE_PREFIX_PATH}/bin" "$ENV{Qt6_DIR}/../../../bin"
        PATHS ENV PATH
    )
    if(WINDEPLOYQT_EXECUTABLE)
        # Install-time: bundle Qt runtime into the package (CPack ZIP/NSIS)
        install(CODE "
            execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations --no-system-d3d-compiler --no-opengl-sw \"\${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}.exe\")
        ")
        # Build-time: make ./build/PDFToPlainText.exe runnable immediately (no manual PATH needed)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-system-d3d-compiler --no-opengl-sw "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Running windeployqt — build dir is now self-contained"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found — build dir will require Qt bin dir on PATH at runtime")
    endif()

    # Build-time: copy Poppler + MinGW runtime DLLs that windeployqt does not handle
    # Works for MSYS2 (C:/tools/msys64/ucrt64/bin) and vcpkg (x64-mingw-dynamic) layouts
    # Hardcoded list covers the usual runtime; GLOB handles versioned ICU etc.
    foreach(_dll IN ITEMS libpoppler-qt6-3.dll libpoppler-163.dll libpoppler-cpp-3.dll
                          libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
                          liblcms2-2.dll libdouble-conversion.dll
                          libicuin78.dll libicuuc78.dll libicudt78.dll
                          libharfbuzz-0.dll libpng16-16.dll libfreetype-6.dll
                          libb2-1.dll libgraphite2.dll libpcre2-16-0.dll libpcre2-8-0.dll
                          zlib1.dll libbrotlidec.dll libbrotlicommon.dll libzstd.dll
                          libwebp-7.dll libjpeg-8.dll libtiff-6.dll
                          nss3.dll libnspr4.dll libplc4.dll libplds4.dll
                          nssckbi.dll nssdbm3.dll nssutil3.dll smime3.dll ssl3.dll softokn3.dll freebl3.dll
                          libmd4c.dll libmd4c-html.dll libsharpyuv-0.dll
                          libglib-2.0-0.dll libbz2-1.dll libopenjp2-7.dll libdeflate.dll libjbig-0.dll libLerc.dll
                          libcurl-4.dll libintl-8.dll liblzma-5.dll libiconv-2.dll libunistring-5.dll
                          libidn2-0.dll libpsl-5.dll libssh2-1.dll libnghttp2-14.dll libnghttp3-9.dll
                          libngtcp2-16.dll libngtcp2_crypto_ossl-0.dll libcrypto-3-x64.dll libssl-3-x64.dll)
        # First try known prefixes, then PATH
        find_file(_found_${_dll} ${_dll}
            HINTS "${CMAKE_PREFIX_PATH}/bin" "C:/tools/msys64/ucrt64/bin" "C:/tools/msys64/mingw64/bin"
            PATHS ENV PATH
            NO_DEFAULT_PATH
        )
        find_file(_found_${_dll} ${_dll}
            HINTS "${CMAKE_PREFIX_PATH}/bin"
            PATHS ENV PATH
        )
        if(_found_${_dll})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_found_${_dll}}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "Copying ${_dll} to build dir"
                VERBATIM
            )
            unset(_found_${_dll} CACHE)
        endif()
    endforeach()
    # Version-agnostic fallback: copy any libicu*.dll / liblcms2*.dll / nss*.dll that exists (covers ICU 78, 79, etc.)
    foreach(_pattern IN ITEMS "libicu*.dll" "liblcms2*.dll" "nss*.dll" "libnspr*.dll" "libplc*.dll" "libplds*.dll" "smime*.dll" "ssl*.dll" "softokn*.dll" "freebl*.dll" "*md4c*.dll" "libglib*.dll" "libbz2*.dll" "libopenjp2*.dll" "libdeflate*.dll" "libjbig*.dll" "libLerc*.dll" "libsharpyuv*.dll" "libcurl*.dll" "libintl*.dll" "liblzma*.dll" "libiconv*.dll" "libidn*.dll" "libpsl*.dll" "libssh*.dll" "libnghttp*.dll" "libngtcp*.dll" "libcrypto*.dll" "libssl*.dll" "libunistring*.dll")
        foreach(_g IN LISTS _glob_dlls)
            get_filename_component(_g_name "${_g}" NAME)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_g}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "Copying ${_g_name} (glob) to build dir"
                VERBATIM
            )
        endforeach()
    endforeach()

    # Generic fallback: CMake 3.21+ can enumerate RUNTIME_DLLS for imported targets (covers vcpkg)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}> 2>nul || ${CMAKE_COMMAND} -E true
            COMMAND_EXPAND_LISTS
            COMMENT "Copying TARGET_RUNTIME_DLLS if any"
        )
        # Ultimate fallback: GET_RUNTIME_DEPENDENCIES recursively finds nss/nspr etc. without hardcoding versions.
        # NOTE: every -D argument is fully quoted: embedded quotes do NOT group
        # in CMake language, so -DSEARCH_DIRS="a;b" would split on the semicolon
        # and arrive as a single quoted entry plus swallowed positionals.
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} "-D" "TARGET_FILE=$<TARGET_FILE:${PROJECT_NAME}>" "-D" "DEST_DIR=$<TARGET_FILE_DIR:${PROJECT_NAME}>" "-D" "SEARCH_DIRS=${CMAKE_PREFIX_PATH}/bin;C:/tools/msys64/ucrt64/bin;C:/tools/msys64/mingw64/bin" "-P" "${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeDeps.cmake"
            COMMAND_EXPAND_LISTS
            COMMENT "Copying transitive runtime DLLs (GET_RUNTIME_DEPENDENCIES)"
        )
    endif()

    # Install-time: ship the runtime DLLs the POST_BUILD step placed next to the
    # executable. Copying from the build dir — instead of re-resolving the
    # closure or checking hardcoded toolchain paths — is what makes the
    # installed app and CPack packages self-contained on every machine:
    # the build dir already holds the exact, verified closure (windeployqt's
    # Qt DLLs plus CopyRuntimeDeps' Poppler/MinGW DLLs) that the exe runs
    # against. Re-resolving at install time was tried and rejected:
    # file(GET_RUNTIME_DEPENDENCIES) needs dumpbin/objdump on the just-written
    # binary and fails intermittently right after windeployqt touches it,
    # and fatals on "conflicting paths" once Qt DLLs sit beside the exe.
    # (Single-config layouts: with Ninja/Makefiles the DLLs sit directly in
    # CMAKE_BINARY_DIR. No-op on Linux/macOS, which produce no .dll files.)
    install(DIRECTORY "${CMAKE_BINARY_DIR}/" DESTINATION bin
        FILES_MATCHING PATTERN "*.dll")
endif()

# macOS: bundle Qt frameworks so the .app runs on other machines
if(APPLE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
        HINTS "${Qt6_DIR}/../../../bin" "$ENV{QT_ROOT_DIR}/bin"
    )
    if(MACDEPLOYQT_EXECUTABLE)
        message(STATUS "macdeployqt: ${MACDEPLOYQT_EXECUTABLE}")
        # NOTE: no extra flags beyond -verbose. A bogus flag here (we once
        # passed a nonexistent -qm) makes macdeployqt exit nonzero, and since
        # execute_process used to ignore the result, installs silently shipped
        # an .app with no Qt frameworks. The RESULT_VARIABLE check below turns
        # any future failure into a loud, immediate error instead.
        install(CODE "
            execute_process(
                COMMAND \"${MACDEPLOYQT_EXECUTABLE}\" \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app\" -verbose=1
                RESULT_VARIABLE _macdeployqt_rc
            )
            if(NOT _macdeployqt_rc EQUAL 0)
                message(FATAL_ERROR \"macdeployqt failed with exit code \${_macdeployqt_rc} - the .app would ship without Qt frameworks\")
            endif()
        ")
    else()
        message(STATUS "macdeployqt not found - .app will not bundle Qt frameworks")
    endif()
endif()

# --- CPack (one-click installers) --------------------------------------------
set(CPACK_PACKAGE_NAME "PDFToPlainText")
set(CPACK_PACKAGE_VENDOR "PDFToPlainText")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lightweight PDF to plain text converter")
set(CPACK_PACKAGE_CONTACT "https://github.com/YousefOnWeb/PDF_To_Plain_Text")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PDFToPlainText")

# Generators per platform
if(WIN32)
    find_program(NSIS_MAKENSIS makensis)
    if(NSIS_MAKENSIS)
        set(CPACK_GENERATOR "NSIS;ZIP")
    else()
        set(CPACK_GENERATOR "ZIP")
        message(STATUS "NSIS makensis not found - building ZIP only (install NSIS from https://nsis.sourceforge.io or via pacman -S mingw-w64-x86_64-nsis / choco install nsis)")
    endif()
    set(CPACK_NSIS_DISPLAY_NAME "PDF To Plain Text")
    set(CPACK_NSIS_PACKAGE_NAME "PDFToPlainText")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    if(EXISTS "${APP_ICON_ICO}")
        # Installer/uninstaller icon and Start-menu shortcut icon
        set(CPACK_NSIS_MUI_ICON "${APP_ICON_ICO}")
        set(CPACK_NSIS_MUI_UNIICON "${APP_ICON_ICO}")
        set(CPACK_PACKAGE_ICON_MUI "${APP_ICON_ICO}")
    endif()
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/${PROJECT_NAME}.exe")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\PDF To Plain Text.lnk' '$INSTDIR\\\\bin\\\\${PROJECT_NAME}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$M_STARTMENU\\\\PDF To Plain Text.lnk'")
elseif(APPLE)
    # Distinct artifact names per CPU architecture so the arm64 and x86_64
    # releases coexist as separate downloads. Derived from the arch CMake is
    # actually building — CI passes -DCMAKE_OSX_ARCHITECTURES explicitly, so
    # there is nothing to keep in sync by hand.
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" AND CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(_mac_arch "universal")
    elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
        set(_mac_arch "x86_64")
    else()
        set(_mac_arch "arm64")
    endif()
    set(CPACK_SYSTEM_NAME "Darwin-${_mac_arch}")
    set(CPACK_GENERATOR "DragNDrop;ZIP")
    set(CPACK_DMG_VOLUME_NAME "PDFToPlainText")
    set(CPACK_DMG_FORMAT "UDZO")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "PDFToPlainText")
    # Only display-server prerequisites stay on the system (bundling mesa
    # libGL would break vendor driver dispatch); Qt, Poppler and everything
    # else ship inside the package (see the Linux block above).
    # libxcb-cursor0 is load-bearing since Qt 6.5: the xcb platform plugin
    # fails to load without it. The remaining libxcb*/libxkbcommon-x11 libs
    # are the rest of libqxcb.so's runtime closure; libdbus-1-3 covers the
    # bundled QtDBus linkage (BundleLinuxDeps.cmake excludes libdbus).
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libx11-6, libxcb1, libxkbcommon0, libxcb-cursor0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render-util0, libxcb-shape0, libxkbcommon-x11-0, libdbus-1-3")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    # RPM can be added on demand; enable with -DCPACK_GENERATOR="TGZ;DEB;RPM"
endif()

# Source package
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/out/;/.git/;/.github/;/.vs/;CMakeCache")

include(CPack)

