# PDF To Plain Text — Product Specification

> Living document. Written incrementally in numbered steps; each step is committed separately.
> Progress: Step 6 of 6 done. All parts complete.
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

Where Part 2 describes what the user sees, this part defines exact behavior:
what is accepted, what is produced, in what order, and what happens on every
failure. Selection visuals and control styling stay in Part 2.

### 3.1 Input acceptance

- A path enters the queue only if it is a local file, ends in `.pdf`
  (case-insensitive), exists, and is not already queued as the identical path
  string (`src/DropFrame.cpp:30-32,70-82`, `src/MainWindow.cpp:387-397`).
- Drops and picker selections share one entry point (`addFiles`), so both obey
  the same rules; mixed drops queue the PDFs and silently skip the rest.
- The picker dialog filters to `PDF Files (*.pdf)` and starts in the home
  folder (`src/MainWindow.cpp:435-439`). Dragged non-local URLs (e.g. from a
  browser) are skipped (`src/DropFrame.cpp:72`).
- Queue order is insertion order. There is no sorting and no manual reorder;
  processing follows queue order (§3.3).

### 3.2 Queue lifecycle

- The queue is an in-memory `QStringList` mirrored 1:1 by list rows carrying
  the path in `Qt::UserRole` (`src/MainWindow.h:83`, `src/MainWindow.cpp:394-399`).
- Removals (single `×`, Remove Selected with descending-row deletion so indices
  stay valid, Clear) update model and view together and refresh the empty
  placeholder, selection state, and Extract enablement
  (`src/MainWindow.cpp:561-581`, `src/MainWindow.cpp:543-559`).
- A removal never touches the disk: no confirmation modal exists by design;
  the 7-second undo (§2.5) is the safeguard.
- On **fully successful** conversion the queue is cleared (with undo available,
  `src/MainWindow.cpp:519-528`). On **partial or total failure the queue is
  preserved** — failed rows stay visible, marked (§2.4), so the user can fix
  the cause and press Extract again without re-queuing.

### 3.3 Extraction semantics

One run processes a snapshot taken at button-press time (file list plus output
directory are copied, `src/MainWindow.cpp:470-474`); later queue edits cannot
affect a run, and all queue/output controls are disabled for its duration.

Per file, in order (`src/PdfExtractor.cpp:34-87`):

1. **Load** via `Poppler::Document::load`. A null document means the file is
   encrypted/corrupt/unreadable → failure path (§3.4). A non-null but locked
   document means password protection → its own failure message.
   Render antialiasing hints are explicitly disabled: irrelevant to text and
   wasted work (`src/PdfExtractor.cpp:53-55`).
2. **Open output** `<outputDir>/<completeBaseName>.txt` for writing, truncating
   any previous file of the same name **without prompting**. `completeBaseName`
   strips only the last suffix, so `report.final.pdf` becomes `report.final.txt`
   (`src/PdfExtractor.cpp:37`). An unopenable output is a per-file failure
   carrying the OS error string.
3. **Pages 0..N−1 in order.** Each page is held in a `unique_ptr` (freed
   immediately after use, bounding memory on huge documents) and its text taken
   with `page->text(QRectF())` — the call that strips images, vector graphics,
   and formatting down to raw Unicode (`src/PdfExtractor.cpp:69-75`). Null
   pages are skipped; empty pages contribute nothing; otherwise the page text
   is written plus a trailing newline separator if it lacks one, so pages never
   fuse into one line (`src/PdfExtractor.cpp:76-80`).
4. **Stream encoding is UTF-8** (`QStringConverter::Utf8`,
   `src/PdfExtractor.cpp:66`); the file is opened with `QIODevice::Text`, so
   line endings follow OS convention on write.
5. Success emits `fileSucceeded(pdfPath, txtPath)`; either way
   `progressChanged(i+1, total)` fires, then the loop continues with the next
   file. One bad file never aborts the run.

After the loop, `finished(succeeded, failed)` fires once
(`src/PdfExtractor.cpp:89`). There is **no cancellation**: no cancel control
exists, and closing the window mid-run quits the worker thread
(`src/MainWindow.cpp:348-354`).

### 3.4 Failure taxonomy

Exactly three per-file failure messages exist, all surfaced verbatim in the
status label, the row tooltip, the copy-to-clipboard payload, and the global
popover (§2.4, §2.9):

| Condition | Message (`src/PdfExtractor.cpp`) |
|---|---|
| `Document::load` returns null | `Failed to open PDF (encrypted or corrupted).` (:42) |
| Document loads but `isLocked()` | `PDF is password-protected.` (:48) |
| Output file cannot be opened | `Cannot write output file: <OS error>.` (:59) |

Scanned-image PDFs are **not** failures: they load and convert normally,
yielding an empty (or near-empty) `.txt`, consistent with the no-OCR non-goal
(§1.5). A missing output folder is handled before the run: the app creates it,
warning only if creation fails (`src/MainWindow.cpp:455-461`).

### 3.5 Progress reporting

- Granularity is **per file**, not per page: `progressChanged(completed, total)`
  fires exactly once per queued file plus the final `finished`
  (`src/PdfExtractor.h:16-20`).
- The bar's range is reset to the run's file count at start
  (`src/MainWindow.cpp:484-487`), so it always reads "`completed / total files
  (percent)`" truthfully, including runs where files fail (failed files still
  advance the counter, §3.3 step 5).

### 3.6 Output folder and persistence

- The output folder is the **only** persistent setting, stored under
  organization `PDFToPlainText`, application `PDFToPlainText`, key `outputDir`
  (`src/main.cpp:10`, `src/MainWindow.cpp:356-364`).
- Startup rule: use the saved path iff non-empty **and** still existing;
  otherwise fall back to `DocumentsLocation`, or the home folder if even that
  is unavailable (`src/MainWindow.cpp:375-379`). A stale saved path (deleted
  or unplugged drive) therefore degrades to Documents, never to an error.
- Changing the folder writes the setting immediately
  (`src/MainWindow.cpp:367-373`). The display field is read-only (§2.6), so
  the stored value and the shown value cannot diverge.

### 3.7 Completion outcomes

Reported by `onExtractionFinished` (`src/MainWindow.cpp:514-541`):

- **All succeeded:** green "Converted N file(s) successfully.", queue cleared
  (undoable), Extract disabled, failure map cleared.
- **Partial:** yellow "Converted S file(s), F failed. Check output folder:
  `<dir>`.", queue and failed-row markings preserved for retry; the general
  status retains this summary.
- **All failed:** red "Conversion failed for N file(s).", queue preserved.
- The global warning button appears iff the failure map is non-empty and hides
  when a new run starts or a full success clears it
  (`src/MainWindow.cpp:463-464,525,538,657-663`).

### 3.8 Undo data rules

- Each removal stores paths plus original row indices (`UndoState`,
  `src/MainWindow.cpp:25-31`). Undo re-inserts ascending by index, clamped
  into range, rebuilding fully wired rows (`src/MainWindow.cpp:589-600`).
- Undo state is **single-shot and single-level**: any new removal overwrites
  it, a successful undo clears it, and it never survives an application
  restart. "Undo successful" shows 4 s, then the status resets
  (`src/MainWindow.cpp:645-655`).

### 3.9 Deliberate non-behaviors (functional)

- Re-running overwrites same-named `.txt` files silently (§3.3 step 2).
- No duplicate detection in the *output* folder; no skip-if-exists logic.
- No mid-run cancellation, pause, or per-file retry button (retry = press
  Extract again; the queue is already intact).
- No post-run "open output folder" action and no summary beyond the status
  label plus popover.

## Part 4 — Technical architecture

### 4.1 Module map

Five units, no other sources (`CMakeLists.txt:65-76`):

| Unit | Files | Role |
|---|---|---|
| Bootstrap | `src/main.cpp` | App metadata, icon, theme, event loop. No business logic. |
| Orchestrator + UI | `src/MainWindow.h/.cpp` | Builds every widget, owns the queue model, owns the worker thread, translates worker signals into UI state. Contains zero extraction logic. |
| Drop zone | `src/DropFrame.h/.cpp` | `QFrame` subclass: drag filtering/hover feedback, click-to-browse. Emits paths; never touches the queue. |
| Worker | `src/PdfExtractor.h/.cpp` | Stateless `QObject` living on the worker thread: the §3.3 pipeline. Never touches widgets. |
| Queue row | `src/FileRowWidget.h/.cpp` | Per-file row widget: label, hover-remove, failure icon, selection forwarding. Knows one path (`src/FileRowWidget.h:25`). |

Dependency direction is strict: rows and the drop zone emit upward to
`MainWindow`; the worker is invoked by `MainWindow` and reports back by
signal. Nothing reaches sideways or downward into another unit's internals.

### 4.2 Bootstrap (`src/main.cpp`)

Order is load-bearing: metadata first (`setApplicationName`,
`setOrganizationName`, `setOrganizationDomain`, `setApplicationVersion`,
:9-12 — the organization/app names are what `QSettings` keys off, §3.6),
then the window icon from the compiled-in resource
(`:/icons/app.png`, :17; see §4.9), then the Fusion style plus the full dark
`QPalette` (:20-34), then `MainWindow::show()` and `exec()` (:36-38).

### 4.3 MainWindow as orchestrator

`MainWindow` constructs the entire widget tree in its constructor
(`src/MainWindow.cpp:33-346`), wires every connection (:328-345), starts the
worker thread, loads settings, and initializes placeholder/selection state
(:317-326). Thereafter it only reacts: UI events update the model and enable
states; worker signals update progress, rows, and status. The `destroyed` →
thread-quit connection (:339) plus quit/wait/`deleteLater` in the destructor
(:348-354) define the only shutdown path.

### 4.4 Threading model

- One dedicated `QThread` for the application's lifetime; the single
  `PdfExtractor` instance is moved onto it at startup
  (`src/MainWindow.cpp:317-321`).
- A run is dispatched with `QMetaObject::invokeMethod` + `Qt::QueuedConnection`
  carrying a lambda that captures **copies** of the file list and output dir
  (`src/MainWindow.cpp:472-474`). The worker therefore needs no locks and no
  access to the window; the snapshot rule (§3.3) falls out of the capture.
- All worker→GUI signals (`progressChanged`, `fileSucceeded`, `fileFailed`,
  `finished`, `src/PdfExtractor.h:16-20`) cross threads via automatic queued
  connections. Affinity rule, enforced by construction: the worker never names
  a widget, and the GUI thread never blocks on the worker — the window stays
  responsive by design, not by tuning.
- `QtConcurrent` is a linked dependency (`CMakeLists.txt:152`) but the
  dispatch mechanism above is what the code actually uses.

### 4.5 Signal/slot contracts

| Signal | Emitter | Receiver / effect |
|---|---|---|
| `filesDropped(QStringList)` | `DropFrame` (:76) | `MainWindow::onDropFiles` → `addFiles` |
| `browseRequested()` | `DropFrame` (:86) | `MainWindow::onBrowseClicked` → picker |
| `removeRequested(FileRowWidget*)` | `FileRowWidget` (:54) | Row lookup by widget identity → single remove + undo |
| `progressChanged(int, int)` | worker, per file | Progress bar range/value |
| `fileSucceeded(pdf, txt)` | worker | Currently a no-op hook (:495-498); row needs no change on success |
| `fileFailed(pdf, error)` | worker | Failure map insert, row marked failed, status text, global button refresh |
| `finished(succeeded, failed)` | worker, once | The three completion outcomes (§3.7) |
| `itemSelectionChanged` | `QListWidget` | `onSelectionChanged` → Remove-Selected enablement |

### 4.6 DropFrame event mechanics (`src/DropFrame.cpp`)

- `dragEnterEvent` accepts **only** when at least one URL is a local `.pdf`;
  anything else is ignored, so the OS shows the correct cursor (:34-57).
- Accepting sets a dynamic `dragHover` property and repolishes the style to
  swap the stylesheet branch (:47-53); `dropEvent` clears it the same way
  (:63-68). `paintEvent` re-runs the style primitive so the rounded dashed
  border paints correctly (:91-98).

### 4.7 FileRowWidget mechanics (`src/FileRowWidget.cpp`)

- The text label is deliberately mouse-transparent (`NoTextInteraction` +
  `WA_TransparentForMouseEvents`, :25-26) so clicks anywhere in the row reach
  `mousePressEvent`, which forwards selection to the hosting `QListWidgetItem`
  with Ctrl-toggle / Shift-range / plain-select semantics, excepting clicks
  that land on either icon button (:104-143).
- The `×` button shows on hover only (`enterEvent`/`leaveEvent`, :76-84);
  the `⚠` button shows if and only if the row is failed (`setFailed`, :86-102),
  which also swaps the row's text color and background tint.
- Clicking `⚠` copies the stored error string to the system clipboard, briefly
  retitles its own tooltip, and flashes the main window's `CopyToast` (found by
  object name, 2 s auto-hide, :55-69).

### 4.8 External dependencies and discovery

- **Qt 6.2+**, components Widgets + Concurrent, found via `find_package`
  (`CMakeLists.txt:24`); `CMAKE_AUTOMOC` is on (:10), and Windows configure
  notes document the `moc.exe`-on-`PATH` requirement (:26-43).
- **Poppler-Qt6** via two paths: a `Poppler` config package with the `Qt6`
  component first (vcpkg-style), else `pkg-config poppler-qt6`
  (`CMakeLists.txt:46-52`), with a warning naming every supported install
  route when neither is found (:54-62). The source picks its header with
  `__has_include`, preferring `<poppler-qt6.h>`
  (`src/PdfExtractor.cpp:10-20`).
- Language standard C++17 (`CMakeLists.txt:7-8`); memory discipline is
  `unique_ptr` per document/page, so peak memory is one page, not one document
  (§3.3 step 3).

### 4.9 Icon, version, and resource wiring

- `assets/app.qrc` compiles `app.png` into the binary; the runtime window icon
  comes from `:/icons/app.png` on every OS (`src/main.cpp:17`).
- **Windows:** a generated `.rc` supplies both the exe icon and `VERSIONINFO`
  from the same `project(VERSION)` as the installers (`CMakeLists.txt:92-136`).
  It is hand-written rather than `RC_ICONS` because Qt links
  `libQt6EntryPoint.a`, which already defines `IDI_ICON1` — Windows shows only
  the first group, so ours must arrive via a linked object that precedes the
  archive (:87-91). `windres` gets numeric constants instead of SDK symbols,
  which it cannot resolve (:106-107).
- **macOS:** a real `.app` bundle carrying `app.icns`, bundle name, identifier,
  and version fields, with `@executable_path/../Frameworks` rpath
  (`CMakeLists.txt:139-150`).
- **Version single-sourcing:** `PDFTOTEXT_VERSION` is injected as a compile
  definition from `project(VERSION)` (`CMakeLists.txt:156-158`) and read by
  `setApplicationVersion` (`src/main.cpp:12`), so the in-app version string
  cannot drift from installer filenames (enforced in CI per Part 5).

## Part 5 — Build, packaging, distribution, CI/CD

### 5.1 Toolchains and prerequisites

One compiler family per OS — GCC / MinGW-w64 / Clang — plus CMake ≥ 3.21,
Qt 6.2+ (Widgets + Concurrent), and Poppler with Qt6 bindings. How each is
obtained is the reader's choice; `README.md` lists common routes without
mandating any. What CI itself uses (`.github/workflows/ci.yml:55-155`) is the
reference configuration:

| OS | Compiler | Qt 6.8.2 | Poppler-Qt6 | Extras |
|---|---|---|---|---|
| Windows (`windows-latest`, MINGW64) | `mingw-w64-x86_64-gcc` | `mingw-w64-x86_64-qt6-base` + `-qt6-tools` via MSYS2 (`ci.yml:56-71`) | `mingw-w64-x86_64-poppler-qt6` | `cmake`, `ninja`, `nsis`, `ntldd`, `pkgconf` from the same repo |
| Linux (`ubuntu-latest`) | GCC | `jurplel/install-qt-action@v4`, `linux_gcc_64` (`ci.yml:74-82`) | `libpoppler-qt6-dev` via apt (`ci.yml:94-98`) | `pkg-config` |
| macOS arm64 (`macos-latest`) | AppleClang | `install-qt-action`, `clang_64` (universal binary, both arches) (`ci.yml:84-92`) | **Built from source**, poppler 26.04.0 with `-DENABLE_QT6=ON` against that Qt plus `-DCMAKE_OSX_ARCHITECTURES=arm64` (`ci.yml:120-155`) | brew cairo/fontconfig/freetype/harfbuzz/jpeg-turbo/libpng/libtiff/little-cms2/nspr/nss/openjpeg/libiconv/zlib/gettext/gperf/ninja/pkgconf/clang-format (prefix `/opt/homebrew`) |
| macOS Intel (`macos-15-intel`, x86_64) | AppleClang | same Qt as arm64 | same source build plus `-DCMAKE_OSX_ARCHITECTURES=x86_64` | same brew list (prefix `/usr/local`, detected via `brew --prefix`, never hardcoded) |

Three macOS facts matter and are all documented in the workflow:

- `brew install poppler` can never work: Homebrew's formula hardcodes
  `-DENABLE_QT6=OFF`, so it ships no `poppler-qt6.pc` (`ci.yml:117-119,
  123-125`).
- `nss` (NSS3 ≥ 3.68, fatal if absent) and `clang-format` (generates the font
  width tables) are genuine build requirements, not optionals.
- Qt 6 still lists the removed Apple Graphics Library in its macOS link
  interface, so the workflow builds an empty arm64 `AGL.framework` stub and
  passes `-F`/`CMAKE_FRAMEWORK_PATH` to both the poppler and app configures;
  Qt renders via Metal/OpenGL and never calls it (`ci.yml:100-115`).

Windows rule with teeth: MSYS2's `ucrt64` and `mingw64` ABIs are incompatible —
Qt, Poppler, and the compiler must all come from the same one, or linking fails
on C++ runtime symbols.

### 5.2 Configuring and building

- Windows, recommended: `powershell -ExecutionPolicy Bypass -File .\build.ps1
  -Clean` from the repo root. The script auto-detects a Qt/Poppler prefix
  (`C:\tools\msys64\ucrt64`, `C:\msys64\ucrt64`, `mingw64` variants,
  `C:\Qt\6.x\mingw_64`, `es.exe` fallback), sets `CMAKE_PREFIX_PATH`, `PATH`
  (required: `moc.exe` needs Qt's `bin` on `PATH` at build time), and
  `PKG_CONFIG_PATH`, then configures (`Ninja`, Release), builds, and packages
  (`build.ps1:18-97`). `-NoPackage` skips CPack; `-Prefix` overrides detection.
- Windows, manual: the `$env:PATH` / `$env:PKG_CONFIG_PATH` lines plus
  `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_PREFIX_PATH="<your-prefix>"` then `cmake --build build --parallel`.
  PowerShell only — never `cmd.exe`.
- Linux/macOS: plain `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` (+
  `-DCMAKE_PREFIX_PATH` pointing at a custom Qt or at `$POPPLER_PREFIX` on
  macOS), then `cmake --build build --parallel`. macOS additionally needs the
  AGL stub flags shown in `ci.yml:169-177` if the local SDK has no AGL.
- `vcpkg.json` exists as an optional manifest route (MinGW triplet
  `x64-mingw-dynamic`). Its poppler feature is named **`qt`**, not `qt6` —
  the wrong name fails with "does not have required feature".

### 5.3 Self-containment: how each OS bundles its runtime

- **Windows:** `windeployqt` runs as a `POST_BUILD` step, so `build\*.exe`
  runs with no `PATH` edits; `cmake/CopyRuntimeDeps.cmake`
  (`GET_RUNTIME_DEPENDENCIES`) copies the Poppler/MinGW closure beside it.
  **Installs ship the build dir's DLLs verbatim** (`install(DIRECTORY
  CMAKE_BINARY_DIR ... *.dll)`): this replaced hardcoded per-DLL
  `install(FILES)` lists, which failed silently on any machine whose toolchain
  lived outside three hardcoded paths and once shipped an installed app with
  only the 4 Qt DLLs. The script takes `CONFLICTING_DEPENDENCIES_PREFIX` so an
  already-present DLL (e.g. from `windeployqt`) is skipped, never fatal.
- **macOS:** the `.app` carries Qt frameworks (deployed by `macdeployqt` at
  install time — invoked with no extra flags after a bogus `-qm` flag once
  made it exit nonzero silently and ship framework-less bundles, now guarded
  by a `RESULT_VARIABLE` check), plus hand-bundled `libpoppler` stack with
  **SONAME symlinks preserved** (`cp -R`; copying only versioned files
  orphans names like `libpoppler-qt6.3.dylib` and dyld aborts), plus recursive
  Homebrew transitive deps rewritten to `@rpath`, plus the shipped AGL stub.
  A `check_rpath` assertion fails the build if any owned `@rpath` reference
  lacks its file — dyld's own check, run early with precise names.
- **Linux:** no bundling; the `.deb` declares `libqt6widgets6,
  libpoppler-qt6-3` system dependencies. (Validating that closure is an open
  packaging question — see §5.5 and Part 6.)

### 5.4 Installer artifacts (CPack)

One command after building, from `build/` (`cpack --config CPackConfig.cmake
-B ../packages`) or repo root. Per-platform outputs (names from the published
`v0.0.1` release):

| Platform | Artifacts |
|---|---|
| Windows (MinGW) | `PDFToPlainText-<ver>-win64.exe` (NSIS) + `PDFToPlainText-<ver>-win64.zip` |
| macOS | `PDFToPlainText-<ver>-Darwin-arm64.dmg` + `.zip` (Apple Silicon) and `PDFToPlainText-<ver>-Darwin-x86_64.dmg` + `.zip` (Intel); DMGs carry the LICENSE agreement on mount. `CPACK_SYSTEM_NAME` derives from `CMAKE_OSX_ARCHITECTURES`, so the two coexist with nothing to sync by hand |
| Linux | `PDFToPlainText-<ver>-Linux.tar.gz` + `pdftoplaintext_<ver>_amd64.deb` (note: CPack `DEB-DEFAULT` lowercases the name) |

Notes: without `makensis` on `PATH`, CPack falls back to ZIP-only (install
`mingw-w64-x86_64-nsis`, `scoop install nsis`, or `winget install NSIS.NSIS`);
`packages/_CPack_Packages/` is uncompressed build scratch, deleted before
upload (shipping it once exhausted the upload action's Node heap — see §5.5).

### 5.5 CI pipeline (`.github/workflows/ci.yml`)

Triggers: pushes to `main`/`master`, `v*` tags, pull requests, manual dispatch
(`ci.yml:3-9`); four matrix jobs (`windows-mingw`, `linux-gcc`,
`macos-arm64`, `macos-intel`, `ci.yml:21-34`), `fail-fast: false`.

- **Version guard first** (`ci.yml:38-53`): on tag pushes, the tag must equal
  `v<project(VERSION)>` or the run fails in seconds instead of after a full
  three-platform build.
- Setup, configure, build per §5.1–5.2; macOS bundling per §5.3; `cpack`;
  staging-tree deletion; then **smoke tests against the exact shipped
  artifacts** — the layer that has caught every recent packaging defect:
  - *macOS (both arches):* unpack the per-arch Darwin `.zip`
    (`*-Darwin-${arch}.zip`, never the arch-less name — both exist side by
    side; not the `.dmg` either — its license agreement demands interactive
    acceptance and `hdiutil attach` aborts headless), strip quarantine, assert
    zero non-relocatable `otool` references outside `/usr/lib`/`/System`,
    launch 10 s headless (`QT_QPA_PLATFORM=offscreen`), scan output for loader
    errors. Crash `.ips` files upload on failure.
  - *Windows:* expand the `-win64.zip` (the NSIS `.exe` cannot install
    headless — CPack's template hardcodes `RequestExecutionLevel admin` with
    no override, and UAC has nothing to click; same payload either way), gate
    direct DLL dependencies via `ntldd` under a scrubbed `PATH`, then the same
    10 s offscreen launch (`ci.yml:395-454`). The `ntldd` gate is direct-deps
    only: the recursive walk lists virtual entries (`api-ms-win-*`,
    `ext-ms-*`, HVSI/attestation shims) as missing on healthy machines.
  - *Linux:* extract the `-Linux.tar.gz`, gate `ldd` on missing libraries
    (against the runner Qt + system poppler), same 10 s launch (`ci.yml:462-495`).
- **Upload** (`ci.yml:515-530`): only real installer extensions, never
  directories; `NODE_OPTIONS=--max-old-space-size=8192` plus
  `compression-level: 0` (installers are already compressed).
- **Release job** (`ci.yml:532-561`): separate job with `needs: build`, same
  tag condition — one publisher after all three platforms succeed, with
  `fail_on_unmatched_files: true`. An in-matrix release step was removed
  because concurrent publishers race and can ship partial releases.

### 5.6 Release Commit workflow (versioning policy)

`CMakeLists.txt` `project(VERSION)` is the single source of truth; CPack
filenames, the in-app version string (§4.9), and the tag all derive from it:

1. Edit the version in `CMakeLists.txt`.
2. Commit (`chore: bump version to X.Y.Z`).
3. Tag exactly `vX.Y.Z` on that commit.
4. Push branch and tag. CI's version guard plus the gated release job do the rest;
   installers land on the GitHub Releases page with zero manual steps.

### 5.7 Distribution channels

- **GitHub Releases** (`github.com/YousefOnWeb/PDF_To_Plain_Text/releases`):
  every installer, attached automatically per §5.5–5.6.
- **End-user site** (`docs/`, `https://yousefonweb.github.io/PDF_To_Plain_Text/`):
  plain static HTML+CSS, no framework; three OS download cards resolve the
  *latest* release's assets at page-load via the GitHub Releases API (pattern
  match, never hardcoded filenames — including the lowercase `.deb`), with
  static `/releases/latest` fallbacks for no-JS/rate-limit cases.

## Part 6 — Quality attributes, testing, limitations, roadmap

### 6.1 Performance

- The UI thread never waits on conversion: dispatch is one queued call (§4.4),
  and progress granularity is per file (§3.5). A 500-page PDF blocks neither
  scrolling nor selection; per-row removal stays clickable throughout and is
  safe because the worker holds snapshot copies, never the live model.
- Peak conversion memory is one page, not one document (`unique_ptr` per page,
  §3.3 step 3). There is no upper bound on file count or size beyond disk
  space for the outputs.
- No performance numbers are claimed: throughput depends on Poppler and the
  documents. If numbers are ever published, they must name the Poppler
  version, three sample documents, and the machine — otherwise they are
  anecdotes, not specifications.

### 6.2 Reliability

- One bad file never aborts a run (§3.3 step 5); every file advances progress
  and gets its own verdict (§3.7).
- Queue edits are undoable and disk-safe (§2.5, §3.8). The one destructive
  filesystem act — truncating an existing same-named `.txt` — is by design
  (§3.9), not an accident: re-running a queue must converge, not accumulate
  `file (1).txt` clutter.
- Shutdown mid-run quits the worker thread with a bounded 2 s wait
  (`src/MainWindow.cpp:348-354`); a hung extraction therefore delays exit by
  at most that long.

### 6.3 Accessibility and localization

- Contrast: headline >14:1, muted text >7:1 on the window background (§2.12).
  All icon-only controls have text tooltips; all status meaning is also
  carried by text, never color alone.
- Keyboard: row selection uses the list's standard behavior (arrows move,
  Shift extends, Ctrl toggles, all handled with the mouse-forwarding in
  §4.7); every button is a native control reachable by Tab.
- UI language is English only, hardcoded in source. This is a scope decision
  (§1.5's single-setting philosophy extended to strings), not an oversight —
  but it must be revisited deliberately, not drifted into.
- Text *content* is fully Unicode: extraction and output are UTF-8 end to end
  (§3.3 step 4), so German umlauts, CJK, and RTL scripts survive conversion
  whenever the PDF's text layer contains them.

### 6.4 Privacy and security posture

- The app performs no network I/O. Conversion is file-in/file-out on the local
  disk; the only network event in the product's lifetime is the user
  downloading the installer. There is no telemetry, no update check, no
  crash reporting.
- Supply chain is pinned where it matters: Poppler source tarball by version
  (26.04.0), Qt at 6.8.2, vcpkg baseline commit; CI verifies the tag↔CMake
  version match before building (§5.5).
- Known friction, stated plainly: the macOS build is unsigned, so first launch
  requires right-click → Open past Gatekeeper; the Windows installer requests
  elevation (CPack template default, no override variable exists). Neither
  affects the installed app's behavior.

### 6.5 Testing strategy

There is no unit-test suite — stated so it is never assumed. Verification is
layered:

1. **Compile gate:** warnings-visible builds on all three OSs (`-Wall`-class
   output from GCC/Clang/MSVC-style MinGW); the version-guard step fails tag
   builds in seconds on mismatch (§5.5).
2. **Packaging smoke tests** (the layer with the best defect-per-line record
   in this project's history): macOS otool leak check + 10 s offscreen launch
   of the shipped `.zip` payload; Windows `ntldd` direct-deps gate + 10 s
   launch of the shipped payload; Linux `ldd` gate + 10 s launch of the
   tarball. Each tests the artifact users download, with documented
   substitutions where containers can't run headless (ZIP-for-DMG,
   ZIP-for-NSIS-exe). macOS crash `.ips` files upload on failure.
3. **Manual verification habits** (used during development, reproducible by
   any contributor): `build.ps1 -Clean` end to end; `ntldd -R` / `otool -L` /
   `ldd` against the product; launching the result with a bare system `PATH`
   to prove self-containment.
4. **Spec conformance:** Parts 2–3 of this document are the checklist for any
   UI-affecting change — the reviewer walks the new behavior against the
   stated rules before merging.

### 6.6 Known limitations (acknowledged, not queued as bugs)

- Scanned-image PDFs convert to empty files (no OCR, §1.5).
- The Linux `.deb` declares unversioned Qt system dependencies while the
  binary is built against Qt 6.8: on distros shipping older Qt it may refuse
  to run. Bundling Qt on Linux (or versioned deps) is the open packaging
  question noted in §5.3/§5.5.
- No mid-run cancellation, no per-file retry button, no output naming options,
  no drag-reorder of the queue. Each was omitted deliberately (§3.9); each
  needs a proposal, not a bug report, to enter the roadmap.
- `cmd.exe` is unsupported on Windows (PowerShell or MSYS2 shells only);
  pre-release tags like `v0.0.1-rc1` are rejected by the version guard.

### 6.7 Roadmap (candidates, not commitments)

Ordered by value-to-effort as judged today; nothing here is promised:

1. **Cancel button** for in-flight runs (worker loop already iterates per
   file, so a cooperative flag is a small, safe addition).
2. **macOS signing + notarization** (paid Apple Developer account required),
   graduating today's ad-hoc posture (§6.4) to a first-launch without warnings.
3. **Linux Qt bundling** (resolves §6.6's `.deb` question properly).
4. **Output naming options** (e.g. page separators, filename patterns) —
   weighed against the one-setting philosophy; likely rejected or minimal.
5. **Localization framework** (Qt Linguist + `.ts` files) if a second UI
   language is ever actually requested.

### 6.8 Maintaining this document

- UI-affecting change → update Part 2 first, then the behavior in Part 3 if
  semantics moved; keep every `path:line` reference accurate (they rot fast).
- New dependency, flag, or CI stage → Part 5, in the same commit as the change
  when the change alters what a reader must do or expect.
- New defect class found by smoke tests → §6.5's list plus the incident note
  where the fix lives (Part 5), as was done for the hollow-bundle, SONAME,
  and brew-closure incidents.
- Version bumps touch only `CMakeLists.txt` per the Release Commit workflow
  (§5.6); this document names no version except in Part 1's identity table.
