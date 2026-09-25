cmake_minimum_required(VERSION 3.21)
project(PDFToPlainText VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# --- Dependencies -------------------------------------------------------------

# Qt6 is required. Provide Qt6_DIR or CMAKE_PREFIX_PATH if Qt is in a custom location.
# Example: -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
find_package(Qt6 6.2 COMPONENTS Widgets Concurrent REQUIRED)

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
        "  vcpkg:   vcpkg install poppler[qt6]\n"
        "  MSYS2:   pacman -S mingw-w64-x86_64-poppler-qt6\n"
        "  Ubuntu:  sudo apt install libpoppler-qt6-dev\n"
        "  macOS:   brew install poppler --with-qt\n"
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

# Windows: run windeployqt automatically on install (requires Qt bin dir in PATH or QT_HOST_PATH)
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-translations --no-system-d3d-compiler --no-opengl-sw \"\${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}.exe\")
        ")
    endif()
    # Poppler DLLs are handled by vcpkg install step or manual copy; CPack will bundle bin/*.dll
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
