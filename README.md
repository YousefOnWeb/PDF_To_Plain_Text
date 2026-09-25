# PDF To Plain Text

Lightweight, cross-platform desktop utility that converts PDF documents into raw plain text. Drag-and-drop batch processing, persistent output folder, and asynchronous extraction with a real-time progress bar.

Built with **C++17 / Qt 6 / Poppler-Qt6 / CMake** using only open-source toolchains (GCC / MinGW-w64 / Clang).

---

## What it does

- **Frictionless Input** — drop zone accepts single or multiple PDFs, or click to browse.
- **Persistent Workflow** — output folder is remembered via `QSettings`.
- **Asynchronous Performance** — extraction runs in a dedicated `QThread`; UI stays responsive.
- **Pure Text Output** — `Poppler::Page::text()` strips images, graphics, and formatting; writes UTF-8 `.txt` files named after the source PDF.

---

## Prerequisites — get these your preferred way

You need four things. **How** you install them is up to you — pick your preferred package manager. Examples below are just common open-source options (no proprietary tooling required).

| Need | What to get | Example ways to get it (all open-source) |
|------|-------------|------------------------------------------|
| **CMake >= 3.21** | CMake CLI | `cmake.org` installer, `brew install cmake`, `sudo apt install cmake`, MSYS2 `pacman -S mingw-w64-x86_64-cmake` |
| **C++ compiler** | GCC / MinGW-w64 / Clang (all open-source) | MSYS2 `pacman -S mingw-w64-x86_64-gcc`, `sudo apt install build-essential g++`, `brew install gcc`, `clang` from LLVM |
| **Qt 6.2+ (Widgets + Concurrent)** | Qt SDK | MSYS2 `pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-tools`, `aqtinstall` (Python, open-source), `sudo apt install qt6-base-dev`, `brew install qt@6` |
| **Poppler-Qt6** | Poppler with Qt6 bindings | MSYS2 `pacman -S mingw-w64-x86_64-poppler-qt6`, `vcpkg install poppler[qt6]:x64-mingw-dynamic` (MinGW triplet), `sudo apt install libpoppler-qt6-dev`, `brew install poppler` |

> After you install them, **note where they landed** — you will point CMake at those locations in the next section. You do not need to copy DLLs manually; the build does that.

The repository also ships a `vcpkg.json` manifest so `vcpkg` with the MinGW triplet can fetch Poppler automatically.

---

## Build — point CMake at what you installed

All commands below are run **from the repository root** (the folder containing `CMakeLists.txt`) unless noted otherwise.

### 1) Tell CMake where Qt and Poppler are

You only need this if they are not on a system path. Pick the variable that matches your install:

- **Qt in a custom path** (e.g. `C:\Qt\6.8.2\mingw_64` or `C:\msys64\mingw64` or `~/Qt/6.8.2/gcc_64`): pass `-DCMAKE_PREFIX_PATH="<your-qt-path>"`.
- **Poppler via vcpkg (MinGW)**: pass `-DCMAKE_TOOLCHAIN_FILE="<your-vcpkg>/scripts/buildsystems/vcpkg.cmake"` and `-DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic`.

If you installed both via a system package manager (`apt`/`brew`/`pacman` in MSYS2) you can skip both flags — `find_package` and `pkg-config` find them automatically.

### 2) Configure and build

#### Windows (PowerShell only, open-source toolchain)

Open **PowerShell** from the repository root. Uses the open-source **MinGW-w64 GCC** toolchain.

```powershell
# From: <repo-root>\  (where CMakeLists.txt lives)

# Example A — Qt + Poppler both from MSYS2 MINGW64 (no extra flags needed inside MINGW64 shell):
# If you are in MSYS2 MINGW64, just run:
# cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Example B — Qt via aqtinstall + Poppler via vcpkg (MinGW triplet):
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.8.2\mingw_64" `
  -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic

cmake --build build --config Release --parallel
.\build\PDFToPlainText.exe   # From: <repo-root>\  — run the app
```

> Do **not** use `cmd.exe`. Run PowerShell or the MSYS2 `MINGW64` shell. All Windows builds use the open-source MinGW-w64 GCC.

If you prefer the MSYS2 shell directly:

```bash
# From: <repo-root>/  inside MSYS2 MINGW64 shell
pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-poppler-qt6 mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/PDFToPlainText.exe
```

#### Linux

```bash
# From: <repo-root>/  (where CMakeLists.txt lives)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# If Qt is in a custom path: add -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.2/gcc_64"
cmake --build build --parallel
./build/PDFToPlainText              # From: <repo-root>/
```

#### macOS

```bash
# From: <repo-root>/  (where CMakeLists.txt lives)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# If Qt is from brew: cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build build --parallel
./build/PDFToPlainText.app/Contents/MacOS/PDFToPlainText
# or: open ./build/PDFToPlainText.app
```

---

## End-user installers (one click)

The project is fully packaged with **CPack**. After a successful build, still from the repository root:

```bash
# From: <repo-root>/build  (the build directory you created)
cpack --config CPackConfig.cmake -B ../packages
# or, from repo root: cpack --config build/CPackConfig.cmake -B packages
```

Outputs per platform (in `<repo-root>/packages/`):

| Platform | Installer |
|----------|-----------|
| **Windows (MinGW)** | `PDFToPlainText-1.0.0-win64.exe` (NSIS, open-source) + `PDFToPlainText-1.0.0-win64.zip` |
| **macOS** | `PDFToPlainText-1.0.0-Darwin.dmg` + `.zip` |
| **Linux** | `PDFToPlainText-1.0.0-Linux.tar.gz` + `PDFToPlainText-1.0.0-Linux.deb` |

On **Windows** the install runs `windeployqt` automatically and bundles Poppler/MinGW DLLs (via `vcpkg` `x64-mingw-dynamic` or MSYS2), so the installed app is self-contained. No manual DLL copying.

---

## CI/CD on GitHub — ready to enable

No setup is required beyond pushing the repository.

1. Push to GitHub (`main` branch). The workflow file is already at `.github/workflows/ci.yml`.
2. Open the **Actions** tab — the `CI` workflow runs on `push` / `pull_request` / `tags`.
3. Each run builds on **Windows (MinGW) + Linux (GCC) + macOS (Clang)**, runs `cpack`, and uploads the installers as **Artifacts** (`packages-windows-mingw`, `packages-linux-gcc`, `packages-macos-clang`).
4. **Release with one click:** push a tag `v*` (e.g. `v1.0.0`):

   ```bash
   # From: <repo-root>/
   git tag v1.0.0
   git push origin v1.0.0
   ```

   The same CI job creates a **GitHub Release** and attaches all installers — ready to download without building locally.

Workflow dependencies are all open-source and cached automatically: MSYS2 + `pacman` for Qt/Poppler on Windows (MinGW), `apt`/`brew` for Poppler on Linux/macOS, and `aqtinstall`/`install-qt-action` with MinGW arch where applicable.

To disable or customize, edit `.github/workflows/ci.yml`.

---

## Project structure

```
.
├── CMakeLists.txt          # find_package(Qt6, Poppler), windeployqt + CPack
├── vcpkg.json              # manifest for Poppler[qt6] (MinGW triplet, optional)
├── src/
│   ├── main.cpp
│   ├── MainWindow.h/.cpp   # layout, QSettings, queue, progress, status
│   ├── DropFrame.h/.cpp    # dashed drop zone, MIME filter, click-to-browse
│   └── PdfExtractor.h/.cpp # Poppler loop, UTF-8 write, worker signals
├── .github/workflows/ci.yml
└── README.md
```

## How it works

1. Drop or browse for PDFs → paths are deduplicated in `MainWindow::m_queuedFiles` and shown in `QListWidget`.
2. Output directory is stored in `QSettings` (`PDFToPlainText/PDFToPlainText`) and defaults to `QStandardPaths::DocumentsLocation`.
3. **Extract Text** invokes `PdfExtractor::extract()` in a dedicated `QThread` via `QMetaObject::invokeMethod(QueuedConnection)`.
4. For each PDF: `Poppler::Document::load()` → iterate `page(i)` → `page->text(QRectF())` → `QTextStream` (UTF-8) → `<basename>.txt`. Progress and completion are emitted back to the GUI thread.

## License

MIT — see [LICENSE](LICENSE).
