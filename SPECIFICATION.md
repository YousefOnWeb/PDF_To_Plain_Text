# PDF To Plain Text — Product Specification

> Living document. Written incrementally in numbered steps; each step is committed separately.
> Progress: Step 2 of 6 done (Parts 1–2 complete, Parts 3–6 scaffolded).
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

The app is one window (`MainWindow`, a `QMainWindow`, `src/MainWindow.cpp:33`).
There are no secondary windows except system file dialogs and two message boxes
(see §2.10). Regions below are listed top to bottom, matching the layout.

### 2.1 Window frame

- Title: `PDF To Plain Text`; application icon shown in title bar/taskbar/dock
  (`src/main.cpp:17`, plus the multi-resolution `.ico`/`.icns` in `assets/`).
- Default size 720×560, minimum size 640×520 (`src/MainWindow.cpp:35-36`).
  The minimum exists so controls can never be crushed into overlap.
- Single central widget with 20 px margins and 14 px vertical spacing
  (`src/MainWindow.cpp:42-44`). The queue region alone stretches
  (`src/MainWindow.cpp:196`); every other region keeps a fixed height.

### 2.2 Header

- Bold 15 px headline: "Convert PDF documents to plain text."
  (`src/MainWindow.cpp:47-49`).
- One muted 11 px sub-line: "Text is extracted without images or formatting —
  ready for study materials." (`src/MainWindow.cpp:52-54`). Word-wrapped.
- The header is descriptive only; it contains no controls.

### 2.3 Drop zone

A `DropFrame` (`src/DropFrame.h`, `src/DropFrame.cpp:11`): charcoal `#2D2D2D`
panel, minimum height 110, fixed vertical policy, 2 px dashed `#4A5568` border
with 10 px rounded corners, pointing-hand cursor.

- Two centered labels: "Drop PDF files here, or Click to browse." (13 px,
  semibold, `#E0E0E0`) and "Supports batch processing — queue multiple files
  at once." (11 px, `#A0AEC0`) (`src/MainWindow.cpp:66-74`).
- **Drag-and-drop** accepts a drop only if at least one dragged URL is a local
  `.pdf` file (case-insensitive, `src/DropFrame.cpp:30-56`). Non-PDF drops are
  ignored; mixed drops queue the PDFs and silently skip the rest
  (`src/DropFrame.cpp:70-82`). While a valid drag hovers, border and background
  switch to blue/darker (`dragHover`, `src/DropFrame.cpp:23-26`).
- **Click** anywhere on the zone opens the system file picker
  (`src/DropFrame.cpp:84-89` → `src/MainWindow.cpp:435`).
- Disabled while extraction runs (`src/MainWindow.cpp:481`); re-enabled after.

### 2.4 Queue region

Header row first: "Queued files:" label on the left; on the right, a
`Remove Selected` button and a flat red `Clear` button
(`src/MainWindow.cpp:79-97`).

- `Remove Selected` is disabled (greyed, unclickable) until at least one row
  is selected (`src/MainWindow.cpp:627-630`). `Clear` removes everything.
- Below it, a `QStackedWidget` shows exactly one of two pages
  (`src/MainWindow.cpp:107`, `src/MainWindow.cpp:381-385`):
  - **List page:** the `QListWidget` queue (dark `#2D2D2D`, `#4A5568` border,
    minimum height 90, vertical scrollbar as needed, no horizontal scrollbar,
    per-pixel scrolling, `src/MainWindow.cpp:111-125`).
  - **Empty page:** a centered placeholder, "No files queued. Drag files above
    to begin." (`src/MainWindow.cpp:128-137`). The stacked-widget design (not a
    floating overlay) guarantees correct centering on first paint and lets the
    list shrink with a scrollbar instead of overflowing neighboring controls
    when the window is short.

Each queued file is one 28 px row (`FileRowWidget`, `src/FileRowWidget.cpp:13`,
`src/MainWindow.cpp:396`) showing `filename — full-native-path`
(`src/FileRowWidget.cpp:22`), with the full path also as hover tooltip.

- **Selection:** standard desktop behavior implemented per row
  (`src/FileRowWidget.cpp:104-143`) over an `ExtendedSelection` list
  (`src/MainWindow.cpp:113`): plain click selects one, Ctrl/Cmd toggles,
  Shift selects the range. Clicks anywhere inside a row (including on the text
  itself, which is deliberately non-interactive, `src/FileRowWidget.cpp:25-26`)
  select the row; only the two icon buttons consume their own clicks.
  Selected rows use a muted translucent blue
  (`rgba(59,130,246,0.28)` + border, `src/MainWindow.cpp:122-123`).
- **Per-row remove:** a subtle `×` button appears on hover only
  (`src/FileRowWidget.cpp:76-84`) and removes that file (undoable, §2.5).
- **Per-row failure state:** after a failed conversion the row keeps a
  permanently visible red `⚠` icon (`src/FileRowWidget.cpp:86-90`). The row
  text turns muted red on a faint red tint. Hovering the icon shows the exact
  technical reason as a tooltip; clicking it copies that message to the
  clipboard, retitles the tooltip to "Error copied" for 1.5 s, and flashes the
  "Error copied" toast for 2 s (`src/FileRowWidget.cpp:55-69`).

### 2.5 Undo toast and status countdowns

Every queue removal — single hover-`×`, `Remove Selected`, `Clear`, and the
automatic clear after a fully successful conversion — is non-destructive
(queue entries only; files on disk are never touched) and undoable without any
confirmation modal (`src/MainWindow.cpp:583-625`):

- A toast banner appears at the bottom of the queue container: `<what happened>
  — Undo (7s)` plus an `Undo` button (`src/MainWindow.cpp:141-158`,
  `src/MainWindow.cpp:632-643`).
- The bottom-left status label mirrors the countdown second by second
  ("… — Undo available for Ns", `src/MainWindow.cpp:166-187`). After 7 s both
  vanish and the status returns to the general state.
- Pressing `Undo` restores entries at their original positions
  (`src/MainWindow.cpp:589-591`) and shows "Undo successful" for 4 s before
  resetting (`src/MainWindow.cpp:645-655`).

### 2.6 Output folder row

`Output Folder:` label, a read-only line edit showing the current path (with
full-path tooltip), and a `Change...` button (`src/MainWindow.cpp:199-223`).

- The field is read-only so the path can never be half-edited into an invalid
  state; `Change...` (tooltip: "Select where the generated .txt files will be
  saved.") opens the system directory picker, and the choice is saved
  immediately (`src/MainWindow.cpp:445-448`).
- Dark input styling (`#2D2D2D` background, `#E0E0E0` text) keeps the path
  legible on the dark theme. Disabled during extraction.

### 2.7 Progress bar

Hidden unless an extraction is running (`src/MainWindow.cpp:229,483`). While
visible it reads "`completed / total files (percent)`"
(`src/MainWindow.cpp:228`), advancing once per finished file
(`src/MainWindow.cpp:490-493`).

### 2.8 Extract button

Right-aligned primary button, minimum 140×36, blue `#2563EB` with darker hover
and pressed states, greyed when disabled (`src/MainWindow.cpp:239-248`,
tooltip: "Start converting queued PDFs to .txt files.").

- Enabled exactly when the queue is non-empty and no extraction is running
  (`src/MainWindow.cpp:430`, `src/MainWindow.cpp:477-488`).
- With an empty queue it shows an informational message instead of running
  (`src/MainWindow.cpp:451-454`); with a missing output folder it attempts to
  create it, warning only if creation fails (`src/MainWindow.cpp:455-461`).

### 2.9 Status row and global failure summary

Bottom row: an expanding word-wrapped status label plus a warning button
(`src/MainWindow.cpp:252-270`). Status colors encode meaning at a glance:
neutral `#A0AEC0`, working blue `#60A5FA`, success green `#4ADE80`, failure red
`#F87171`, partial-success yellow `#FBBF24`.

- The yellow `⚠` button appears only when at least one file has failed, styled
  to match the yellow partial-failure text as one visual unit
  (`src/MainWindow.cpp:657-663`), with tooltip "`N` file(s) failed — click for
  details".
- Clicking it opens a `Qt::Popup` popover anchored below the button and kept
  inside the screen (`src/MainWindow.cpp:665-689`): title "Failed files" plus
  one `filename: reason` line per failure, mouse-selectable. Clicking elsewhere
  dismisses it — no modal, no workflow interruption.

### 2.10 Dialogs

Only three, all system-native: the PDF picker (filter `PDF Files (*.pdf)`,
starting at the home folder), the output-folder picker (starting at the current
output folder), and two `QMessageBox`es — "Nothing to convert" (info) and
"Output folder missing" (warning). There are deliberately no confirmation
dialogs for queue edits (undo covers them) and no settings dialog (there is
one setting).

### 2.11 Modal states

During extraction (`setExtracting(true)`, `src/MainWindow.cpp:477-488`) the
Extract, Clear, Remove-Selected, Change, and drop-zone controls disable and the
progress bar appears; everything re-enables on completion. The window itself
never blocks: conversion runs on a worker thread (see Part 4).

### 2.12 Visual design system

- Dark theme via Qt Fusion plus a custom palette: window `#1E1E1E`, base
  `#2D2D2D`, text `#E0E0E0`, highlight `#3B82F6` (`src/main.cpp:20-34`).
- Type scale: 15 px bold headline, 13 px semibold drop-zone lead, 11 px labels
  and hints, 11 px status. Muted `#A0AEC0` text on `#1E1E1E` exceeds 7:1
  contrast; headline `#E2E8F0` exceeds 14:1.
- Semantic colors only: blue = primary/progress/selection, red = destructive
  or failed, yellow = partial failure/warning, green = success. No other accent
  colors exist in the UI.
- Every interactive control carries a tooltip; every icon-only button (`×`,
  `⚠`) has a text tooltip explaining it.

### 2.13 Responsive behavior

- The queue container is the sole stretcher; drop zone, folder row, progress,
  button, and status rows are fixed height (`src/MainWindow.cpp:196`).
- The list enforces minimum height 90 and scrolls internally rather than
  pushing controls out of the window (`src/MainWindow.cpp:114-118`); the 640×520
  window minimum is the final backstop (`src/MainWindow.cpp:36`).
- `resizeEvent` needs no manual geometry: the stacked widget keeps the
  placeholder centered automatically (`src/MainWindow.cpp:702-705`).

## Part 3 — Functional specification

*(Step 3 — not written yet.)*

## Part 4 — Technical architecture

*(Step 4 — not written yet.)*

## Part 5 — Build, packaging, distribution, CI/CD

*(Step 5 — not written yet.)*

## Part 6 — Quality attributes, testing, limitations, roadmap

*(Step 6 — not written yet.)*
