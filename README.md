# PDF To Plain Text

Lightweight, cross-platform desktop utility that converts PDF documents into raw plain text. Drag-and-drop batch processing, persistent output folder, and asynchronous extraction with a real-time progress bar.

Built with **C++17 / Qt 6 / Poppler-Qt6 / CMake**.

---

## What it does

- **Frictionless Input** — drop zone accepts single or multiple PDFs, or click to browse.
- **Persistent Workflow** — output folder is remembered via `QSettings`.
- **Asynchronous Performance** — extraction runs in a dedicated `QThread`; UI stays responsive.
- **Pure Text Output** — `Poppler::Page::text()` strips images, graphics, and formatting; writes UTF-8 `.txt` files named after the source PDF.

---

## Prerequisites — get these your preferred way

You need four things. **How** you install them is up to you. Examples below are just common options.

| Need | What to get | Example ways to get it |
|------|-------------|------------------------|
| **CMake >= 3.21** | CMake CLI | `cmake.org` installer, `winget install Kitware.CMake`, `brew install cmake`, `sudo apt install cmake`, MSYS2 `pacman -S mingw-w64-x86_64-cmake` |
| **C++ compiler** | MSVC 2022 or MinGW-w64 (Windows), GCC/Clang (Linux/macOS) | Visual Studio Installer, `winget install Microsoft.VisualStudio.2022.BuildTools`, `brew install llvm`, `sudo apt install build-essential` |
| **Qt 6.2+ (Widgets + Concurrent)** | Qt SDK | **Qt Maintenance Tool** from `qt.io` (official), `aqtinstall`, `brew install qt@6`, `sudo apt install qt6-base-dev`, `winget install Qt.Qt` |
| **Poppler-Qt6** | Poppler with Qt6 bindings | `vcpkg install poppler[qt6]`, MSYS2 `pacman -S mingw-w64-x86_64-poppler-qt6`, `sudo apt install libpoppler-qt6-dev`, `brew install poppler` |

> After you install them, **note where they landed** — you will point CMake at those locations in the next section. You do not need to copy DLLs manually; the build does that.

The repository also ships a `vcpkg.json` manifest so `vcpkg` can fetch Poppler automatically when you use the vcpkg toolchain.

---

## Build — point CMake at what you installed

All commands below are run **from the repository root** (the folder containing `CMakeLists.txt`) unless noted otherwise.

### 1) Tell CMake where Qt and Poppler are

You only need this if they are not on a system path. Pick the variable that matches your install:

- **Qt in a custom path** (e.g. `C:\Qt\6.8.2\msvc2022_64` or `~/Qt/6.8.2/gcc_64`): pass `-DCMAKE_PREFIX_PATH="<your-qt-path>"`.
- **Poppler via vcpkg**: pass `-DCMAKE_TOOLCHAIN_FILE="<your-vcpkg>/scripts/buildsystems/vcpkg.cmake"` and `-DVCPKG_TARGET_TRIPLET=x64-windows` (Windows) or `x64-linux`/`arm64-osx`.

If you installed both via a system package manager (`apt`/`brew`/`pacman`) you can skip both flags — `find_package` and `pkg-config` find them automatically.

### 2) Configure and build

#### Windows (PowerShell only)

Open **PowerShell** from the repository root.

```powershell
# From: <repo-root>\  (where CMakeLists.txt lives)

# Example A — Qt from Maintenance Tool + Poppler via vcpkg manifest:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.8.2\msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows

# Example B — Qt + Poppler both from MSYS2 (no extra flags needed if msys env is active):
# cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release --parallel
.\build\Release\PDFToPlainText.exe   # From: <repo-root>\  — run the app
```

> Do **not** use `cmd.exe`. The `;` and path handling differ.

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
```

Outputs per platform (in `<repo-root>/packages/`):

| Platform | Installer |
|----------|-----------|
| **Windows** | `PDFToPlainText-1.0.0-win64.exe` (NSIS) + `PDFToPlainText-1.0.0-win64.zip` |
| **macOS** | `PDFToPlainText-1.0.0-Darwin.dmg` + `.zip` |
| **Linux** | `PDFToPlainText-1.0.0-Linux.tar.gz` + `PDFToPlainText-1.0.0-Linux.deb` |

On **Windows** the install runs `windeployqt` automatically and bundles Poppler DLLs when using vcpkg, so the installed app is self-contained. No manual DLL copying.

---

## CI/CD on GitHub — ready to enable

No setup is required beyond pushing the repository.

1. Push to GitHub (`main` branch). The workflow file is already at `.github/workflows/ci.yml`.
2. Open the **Actions** tab — the `CI` workflow runs on `push` / `pull_request` / `tags`.
3. Each run builds on **Windows + Linux + macOS**, runs `cpack`, and uploads the installers as **Artifacts** (`packages-windows-msvc`, `packages-linux-gcc`, `packages-macos-clang`).
4. **Release with one click:** push a tag `v*` (e.g. `v1.0.0`):

   ```bash
   # From: <repo-root>/
   git tag v1.0.0
   git push origin v1.0.0
   ```

   The same CI job creates a **GitHub Release** and attaches all installers — ready to download without building locally.

Workflow dependencies are cached and installed automatically: `jurplel/install-qt-action` for Qt, `apt`/`brew` for Poppler on Linux/macOS, and `lukka/run-vcpkg` + `vcpkg.json` for Poppler on Windows.

To disable or customize, edit `.github/workflows/ci.yml`.

---

## Project structure

```
.
├── CMakeLists.txt          # find_package(Qt6, Poppler), windeployqt + CPack
├── vcpkg.json              # manifest for Poppler[qt6] (used by CI on Windows)
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
