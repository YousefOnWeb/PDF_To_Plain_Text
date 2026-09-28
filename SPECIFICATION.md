# PDF To Plain Text — Product Specification

> Living document. Written incrementally in numbered steps; each step is committed separately.
> Progress: Step 1 of 6 done (Part 1 complete, Parts 2–6 scaffolded).
>
> Source of truth for released behavior is the code in `src/` plus this document.
> Build instructions live in `README.md` and are not repeated here.
> Version numbers live in `CMakeLists.txt` (`project(... VERSION ...)`).

## Contents

- [Part 1 — Product overview](#part-1--product-overview) *(Step 1, done)*
- [Part 2 — User experience specification](#part-2--user-experience-specification) *(Step 2)*
- [Part 3 — Functional specification](#part-3--functional-specification) *(Step 3)*
- [Part 4 — Technical architecture](#part-4--technical-architecture) *(Step 4)*
- [Part 5 — Build, packaging, distribution, CI/CD](#part-5--build-packaging-distribution-cicd) *(Step 5)*
- [Part 6 — Quality attributes, testing, limitations, roadmap](#part-6--quality-attributes-testing-limitations-roadmap) *(Step 6)*

## Conventions used in this document

- **App** means the installed `PDF To Plain Text` desktop program. **Project** means this repository.
- **Must / must not** = guaranteed behavior, verified before release. **Should** = intended behavior with known exceptions, each noted where it applies.
- File references use the form `path:line` (e.g. `src/main.cpp:9`) so a reader can jump from a claim to the code that implements it.
- Anything platform-specific is labeled **Windows**, **macOS**, or **Linux**. Unlabeled claims hold on all three.

---

## Part 1 — Product overview

### 1.1 Identity

| Fact | Value |
|---|---|
| Name | PDF To Plain Text |
| What it is | Single-window desktop utility that converts PDF documents into plain (`.txt`) text files |
| Current version | 0.0.1 (single source of truth: `CMakeLists.txt:5`) |
| License | MIT (`LICENSE`) |
| Repository | `https://github.com/YousefOnWeb/PDF_To_Plain_Text` |
| End-user site | `https://yousefonweb.github.io/PDF_To_Plain_Text/` (`docs/`) |
| Technology | C++17, Qt 6 (Widgets + Concurrent), Poppler-Qt6, CMake ≥ 3.21 |
| Platforms | Windows 10/11 64-bit (MinGW-w64), macOS 11+ arm64, Linux 64-bit (Ubuntu/Debian-family `.deb` plus portable `.tar.gz`) |
| UI language | English only |

### 1.2 Problem statement

Studying from PDF documents routinely requires their *text*: quoting passages, feeding material into flashcard tools, searching across documents, or processing text with other software. The manual path — opening each PDF, selecting all, copying, pasting into a text file, repeating per document — is slow, error-prone (missed pages, pasted headers/footers in the wrong order), and scales linearly with the number of files. Rich PDF content (images, vector graphics, complex layouts) is irrelevant to this task and actively gets in the way.

The app exists to collapse that workflow to: drop files in, choose a folder once, press one button. It deliberately outputs *only* raw string data — no formatting, no images — because that is exactly the input its users' downstream tools accept.

### 1.3 Target users

- **Primary: students working from large PDF study collections** (the originating use case: batches of German study PDFs). They convert many files at once, re-convert updated editions, and care about speed and batch handling more than conversion options.
- **Secondary: anyone who needs plain text out of PDFs** — researchers, archivists, developers preprocessing corpora. They care that output is predictable UTF-8 `.txt` named after the source file.
- **Explicitly not users:** people needing layout-preserving conversion (use a PDF exporter), scanned-page OCR (the app cannot read pixels — see §1.5), or programmatic conversion (there is no CLI or library API; the only interface is the window).

### 1.4 Scope (what the app does)

1. Accept one or many PDF files via drag-and-drop or a file picker.
2. Keep them in a visible, editable queue (add, remove singly, remove selected, clear all, undo).
3. Convert each queued PDF to a UTF-8 `.txt` file named after it, in a user-chosen folder that the app remembers between sessions.
4. Do the conversion off the UI thread with live progress, and report per-file success or the exact failure reason.
5. Ship as a self-contained, one-click installable package per platform, built and published automatically.

### 1.5 Non-goals (what the app does not do)

- **No OCR.** PDFs made of scanned images contain no text layer; the app outputs an empty file for them rather than guessing. This is a documented limitation, not a bug.
- **No layout preservation.** Bold, columns, tables, footnotes order, fonts — all discarded by design.
- **No editing, previewing, merging, or splitting of PDFs.**
- **No network activity.** Conversion is fully offline; the only network use in the project's lifetime is downloading the installer.
- **No CLI, scripting API, or file association.** The window is the entire interface.
- **No settings beyond the output folder.** There is exactly one persistent preference.

### 1.6 Glossary

| Term | Meaning in this document |
|---|---|
| Queue | The in-memory ordered list of PDF paths awaiting conversion, mirrored on screen (`src/MainWindow.h:83`). Cleared on full success, preserved on partial failure. |
| Extraction | Reading a PDF's text layer via Poppler and writing it as UTF-8, page by page (`src/PdfExtractor.cpp:19`). "Extract Text" button starts it. |
| Output folder | The directory receiving `.txt` files. The sole persistent setting, stored via `QSettings` (`src/MainWindow.cpp`, `loadSettings`/`saveOutputDir`). |
| Toast | A small, transient, non-blocking banner (undo, error-copied). Never modal, never steals focus, disappears on its own timer. |
| Popover | The small floating pane anchored to the global warning icon listing failed files. Dismissed by clicking elsewhere (`Qt::Popup`). |
| Self-contained | An installable or build output that runs with no system-wide dependencies beyond the OS itself (all Qt/Poppler/MinGW runtime files ship beside the executable). |
| Release Commit workflow | The project's versioning policy: `CMakeLists.txt` is the single source of truth; a release is a version-bump commit tagged `v<version>`; CI rejects mismatched tags. Details in Part 5. |

---

## Part 2 — User experience specification

*(Step 2 — not written yet.)*

## Part 3 — Functional specification

*(Step 3 — not written yet.)*

## Part 4 — Technical architecture

*(Step 4 — not written yet.)*

## Part 5 — Build, packaging, distribution, CI/CD

*(Step 5 — not written yet.)*

## Part 6 — Quality attributes, testing, limitations, roadmap

*(Step 6 — not written yet.)*
