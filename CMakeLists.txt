cmake_minimum_required(VERSION 3.21)
project(PDFToPlainText VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

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
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Windows: GUI subsystem (no console)
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Concurrent)

if(Poppler_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Poppler::Qt6)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_POPPLER_QT6_CONFIG)
elseif(PopplerQt6_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PopplerQt6_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${PopplerQt6_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PopplerQt6_LIBRARIES})
    target_compile_options(${PROJECT_NAME} PRIVATE ${PopplerQt6_CFLAGS_OTHER})
endif()

# --- Install & Deploy ---------------------------------------------------------
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)

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
                          libwebp-7.dll libjpeg-8.dll libtiff-6.dll)
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
    # Version-agnostic fallback: copy any libicu*.dll / liblcms2*.dll that exists (covers ICU 78, 79, etc.)
    foreach(_pattern IN ITEMS "libicu*.dll" "liblcms2*.dll")
        file(GLOB _glob_dlls "${CMAKE_PREFIX_PATH}/bin/${_pattern}" "C:/tools/msys64/ucrt64/bin/${_pattern}" "C:/tools/msys64/mingw64/bin/${_pattern}")
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
    endif()

    # Install-time: bundle runtime DLLs for ZIP/NSIS (MSYS2 + vcpkg)
    # Prefer explicit install(FILES ...) so CPack ZIP is self-contained even without vcpkg
    foreach(_dll IN ITEMS libpoppler-qt6-3.dll libpoppler-163.dll libpoppler-cpp-3.dll
                          libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
                          liblcms2-2.dll libdouble-conversion.dll
                          libicuin78.dll libicuuc78.dll libicudt78.dll
                          libharfbuzz-0.dll libpng16-16.dll libfreetype-6.dll)
        if(EXISTS "${CMAKE_PREFIX_PATH}/bin/${_dll}")
            install(FILES "${CMAKE_PREFIX_PATH}/bin/${_dll}" DESTINATION bin)
        elseif(EXISTS "C:/tools/msys64/ucrt64/bin/${_dll}")
            install(FILES "C:/tools/msys64/ucrt64/bin/${_dll}" DESTINATION bin)
        elseif(EXISTS "C:/tools/msys64/mingw64/bin/${_dll}")
            install(FILES "C:/tools/msys64/mingw64/bin/${_dll}" DESTINATION bin)
        endif()
    endforeach()
    # Version-agnostic install for ICU/lcms2
    foreach(_pattern IN ITEMS "libicu*.dll" "liblcms2*.dll")
        file(GLOB _inst_glob "${CMAKE_PREFIX_PATH}/bin/${_pattern}" "C:/tools/msys64/ucrt64/bin/${_pattern}")
        if(_inst_glob)
            install(FILES ${_inst_glob} DESTINATION bin)
        endif()
    endforeach()

    # vcpkg: bundle everything in triplet bin
    if(VCPKG_INSTALLED_DIR)
        install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/" DESTINATION bin
            FILES_MATCHING PATTERN "*.dll")
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
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "PDF To Plain Text")
    set(CPACK_NSIS_PACKAGE_NAME "PDFToPlainText")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app.ico")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/app.ico")
    endif()
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/${PROJECT_NAME}.exe")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\PDF To Plain Text.lnk' '$INSTDIR\\\\bin\\\\${PROJECT_NAME}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$M_STARTMENU\\\\PDF To Plain Text.lnk'")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;ZIP")
    set(CPACK_DMG_VOLUME_NAME "PDFToPlainText")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "PDFToPlainText")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6, libpoppler-qt6-3")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    # RPM can be added on demand; enable with -DCPACK_GENERATOR="TGZ;DEB;RPM"
endif()

# Source package
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/out/;/.git/;/.github/;/.vs/;CMakeCache")

include(CPack)
