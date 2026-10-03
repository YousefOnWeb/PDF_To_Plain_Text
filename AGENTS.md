# AGENTS.md — PDF To Plain Text

Instructions for AI coding agents working in this repository. The
human-maintained product spec is `SPECIFICATION.md`; the build guide is
`README.md`. This file is the agent-oriented supplement: how to build,
where things live, and which traps to avoid.

## What this is

Single-window Qt6 desktop utility that converts PDF files to UTF-8
`.txt` files. Offline, no network I/O at runtime, no CLI. Stack:
C++17, Qt 6.2+ (Widgets + Concurrent), Poppler-Qt6, CMake >= 3.21,
CPack installers per OS.

## Source layout

- `src/main.cpp` — bootstrap only: metadata, window icon, dark theme.
- `src/MainWindow.h/.cpp` — UI + orchestration. Owns the queue model,
  the worker thread, settings. Zero extraction logic.
- `src/DropFrame.h/.cpp` — drag-and-drop zone. Emits paths; never
  touches the queue.
- `src/PdfExtractor.h/.cpp` — stateless worker on a `QThread`. The
  Poppler pipeline. Never touches widgets.
- `src/FileRowWidget.h/.cpp` — one queue row. Knows one path.
- `assets/` — `app.png` (runtime icon via `app.qrc`), `PDFToPlainText.ico`
  (Windows), `app.icns` (macOS), `icon-readme.png` (README hero).
- `cmake/CopyRuntimeDeps.cmake` — Windows DLL bundling helper.
- `cmake/BundleLinuxDeps.cmake` — Linux `.so` bundling helper, run at
  install time via `install(CODE ...)`.
- `build.ps1` — one-click Windows build. `docs/` — static download site.

Dependency direction: rows/drop zone emit up to `MainWindow`;
`MainWindow` invokes the worker via queued signals. Keep it that way.

## Versioning (must follow)

`project(VERSION)` in `CMakeLists.txt` is the single source of truth.
Release = bump version, commit, tag `vX.Y.Z`, push both. CI rejects
mismatched tags. Never hardcode version strings elsewhere.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # + -DCMAKE_PREFIX_PATH=<qt> if needed
cmake --build build --parallel
./build/PDFToPlainText
cpack --config build/CPackConfig.cmake -B packages
```

Distro notes: Ubuntu 24.04 has `libpoppler-qt6-dev`; 22.04 needs
Poppler built from source with `-DENABLE_QT6=ON`; Homebrew's poppler
disables Qt6, so macOS also builds Poppler from source. See `README.md`.

## CMake pitfalls (learned the hard way)

- Inside `install(CODE "...")`, `${VAR}` expands at **configure** time;
  `\${VAR}` expands at **install** time (when CPack staging prefixes
  exist). Install-time values (staged exe/lib paths, `RESULT_VARIABLE`s)
  must stay escaped; configure-time values (`CMAKE_BINARY_DIR`,
  `CMAKE_SOURCE_DIR`, `PROJECT_NAME`) must not be. Mixing them up yields
  empty `-D` args and bundling failures — see `cmake_install.cmake`
  when in doubt.
- Never write a file named `qt.conf` into `CMAKE_BINARY_DIR`: with
  single-config generators it lands beside the build-tree exe and
  hijacks its Qt plugin lookup (`Prefix=..` points at the repo root).
  Symptom: `no Qt platform plugin could be initialized`. Stage it under
  a different name and `install(... RENAME qt.conf)`.
- Qt plugin probing must handle distro split layouts, not just
  `<root>/plugins` (Arch: `<root>/lib/qt6/plugins`).
- `file(COPY)` preserves symlinks. When bundling `ldd` results, resolve
  `REALPATH` first or you stage dangling links that fail recursive `ldd`.
- `ldd` tokens containing `/` (e.g. `lib64/ld-linux-*.so.2`) are loader
  paths, never SONAMEs — skip them, don't bundle or symlink them.
- Every staged `.so` needs its own `$ORIGIN` RUNPATH (stamped with
  `patchelf --set-rpath`, a Linux packaging requirement): RUNPATH does
  not propagate to transitive lookups, so a bundled lib whose dependency
  is also bundled (`libpoppler-qt6` → `libpoppler`) still resolves
  `not found` — the exe's `$ORIGIN/../lib` only covers direct deps.
- `ldd` on the exe never sees Qt platform plugins (`plugins/platforms/*.so`
  are `dlopen()`ed): seed them into the bundler queue or their exclusive
  deps (`libQt6XcbQpa` ← `libqxcb.so`) ship missing and only GUI platforms
  fail — offscreen smoke tests stay green and hide it.

## Testing without a display

```bash
QT_QPA_PLATFORM=offscreen timeout 10 ./build/PDFToPlainText
# exit 124 (killed by timeout) = healthy event loop; SIGABRT = broken bundling
ldd build/PDFToPlainText | grep "not found"  # must print nothing
```

The packaged tarball must be tested the same way *without*
`LD_LIBRARY_PATH` — needing it means the `$ORIGIN` rpath regressed.

## Conventions

- Spec references use `path:line`; keep them accurate when moving code.
- The app has exactly one setting (output folder, via `QSettings`).
  Same-named `.txt` outputs are overwritten silently — by design.
- No network code, no telemetry, no threads beyond the one worker.
  Keep it that way.
