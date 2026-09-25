#include "MainWindow.h"
#include "DropFrame.h"
#include "PdfExtractor.h"
#include "FileRowWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>
#include <QThread>
#include <QMessageBox>
#include <QDir>
#include <QStackedWidget>
#include <QTimer>
#include <QResizeEvent>

struct UndoState {
    QStringList paths;
    QList<int> rows;
};

static UndoState s_undoState;
static QTimer *s_undoTimerPtr = nullptr;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("PDF To Plain Text"));
    resize(720, 560);
    setMinimumSize(640, 520);

    auto *central = new QWidget(this);
    central->setObjectName("Central");
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

    // Header — high contrast on dark (#E2E8F0 on #1E1E1E > 14:1)
    auto *header = new QLabel(QStringLiteral("Convert PDF documents to plain text."), this);
    header->setObjectName("HeaderLabel");
    header->setStyleSheet("QLabel#HeaderLabel { font-size: 15px; font-weight: 700; color: #E2E8F0; }");
    root->addWidget(header);

    auto *sub = new QLabel(QStringLiteral("Text is extracted without images or formatting — ready for study materials."), this);
    sub->setStyleSheet("color: #A0AEC0; font-size: 11px;");
    sub->setWordWrap(true);
    root->addWidget(sub);

    // Drop area — dark charcoal, not jarring white
    m_dropFrame = new DropFrame(this);
    m_dropFrame->setMinimumHeight(110);
    m_dropFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *dropLayout = new QVBoxLayout(m_dropFrame);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(4);
    dropLayout->setContentsMargins(12, 12, 12, 12);

    m_dropLabel = new QLabel(QStringLiteral("Drop PDF files here, or Click to browse."), m_dropFrame);
    m_dropLabel->setAlignment(Qt::AlignCenter);
    m_dropLabel->setStyleSheet("color: #E0E0E0; font-size: 13px; font-weight: 600; border: none; background: transparent;");
    dropLayout->addWidget(m_dropLabel);

    auto *hint = new QLabel(QStringLiteral("Supports batch processing — queue multiple files at once."), m_dropFrame);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #A0AEC0; font-size: 11px; border: none; background: transparent;");
    dropLayout->addWidget(hint);

    root->addWidget(m_dropFrame);

    // File list header
    auto *listHeader = new QHBoxLayout();
    auto *queueTitle = new QLabel(QStringLiteral("Queued files:"), this);
    queueTitle->setStyleSheet("font-weight: 600; color: #E2E8F0; font-size: 11px;");
    listHeader->addWidget(queueTitle);
    listHeader->addStretch();
    m_removeSelectedBtn = new QPushButton(QStringLiteral("Remove Selected"), this);
    m_removeSelectedBtn->setToolTip(QStringLiteral("Remove selected files from queue"));
    m_removeSelectedBtn->setEnabled(false);
    m_removeSelectedBtn->setStyleSheet(
        "QPushButton { color: #A0AEC0; font-size: 11px; border: 1px solid #4A5568; border-radius: 4px; padding: 3px 8px; background: #2D2D2D; }"
        "QPushButton:disabled { color: #6B7280; border-color: #3A3A3A; background: #252525; }"
        "QPushButton:!disabled:hover { color: #F87171; border-color: #EF4444; background: #3A3A3A; }");
    listHeader->addWidget(m_removeSelectedBtn);
    m_clearBtn = new QPushButton(QStringLiteral("Clear"), this);
    m_clearBtn->setToolTip(QStringLiteral("Remove all files from the queue."));
    m_clearBtn->setFlat(true);
    m_clearBtn->setStyleSheet("QPushButton { color: #F87171; font-size: 11px; border: none; } QPushButton:hover { color: #EF4444; }");
    listHeader->addWidget(m_clearBtn);
    root->addLayout(listHeader);

    // List container with QStackedWidget for flawless empty placeholder (fixes top-left bug)
    auto *listContainer = new QWidget(this);
    listContainer->setObjectName("ListContainer");
    listContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *listContainerLay = new QVBoxLayout(listContainer);
    listContainerLay->setContentsMargins(0, 0, 0, 0);
    listContainerLay->setSpacing(6);

    m_listStack = new QStackedWidget(listContainer);
    m_listStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Page 0: file list
    m_fileList = new QListWidget(m_listStack);
    m_fileList->setAlternatingRowColors(false);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_fileList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_fileList->setMinimumHeight(90);
    m_fileList->setStyleSheet(
        "QListWidget { border: 1px solid #4A5568; border-radius: 6px; background: #2D2D2D; color: #E0E0E0; }"
        "QListWidget::item { padding: 0px; border: none; background: transparent; margin: 1px 2px; border-radius: 4px; }"
        "QListWidget::item:selected { background: rgba(59, 130, 246, 0.28); border: 1px solid rgba(59,130,246,0.45); }"
        "QListWidget::item:selected:active { background: rgba(59, 130, 246, 0.35); }"
        "QListWidget::item:hover:!selected { background: #3A3A3A; }");
    m_listStack->addWidget(m_fileList);

    // Page 1: empty placeholder — perfectly centered via layout, no viewport geometry hack
    auto *emptyPage = new QWidget(m_listStack);
    emptyPage->setStyleSheet("background: #2D2D2D; border: 1px solid #4A5568; border-radius: 6px;");
    auto *emptyLay = new QVBoxLayout(emptyPage);
    emptyLay->setContentsMargins(12, 12, 12, 12);
    m_emptyPlaceholder = new QLabel(QStringLiteral("No files queued. Drag files above to begin."), emptyPage);
    m_emptyPlaceholder->setAlignment(Qt::AlignCenter);
    m_emptyPlaceholder->setWordWrap(true);
    m_emptyPlaceholder->setStyleSheet("color: #A0AEC0; font-size: 11px; background: transparent; border: none;");
    emptyLay->addWidget(m_emptyPlaceholder, 1, Qt::AlignCenter);
    m_listStack->addWidget(emptyPage);

    listContainerLay->addWidget(m_listStack, 1);

    // Undo toast — transient, non-blocking, at bottom of queue container
    m_undoToast = new QWidget(listContainer);
    m_undoToast->setObjectName("UndoToast");
    m_undoToast->setStyleSheet("QWidget#UndoToast { background: #3A3A3A; border: 1px solid #4A5568; border-radius: 6px; }");
    auto *toastLay = new QHBoxLayout(m_undoToast);
    toastLay->setContentsMargins(10, 6, 10, 6);
    toastLay->setSpacing(8);
    m_undoLabel = new QLabel(m_undoToast);
    m_undoLabel->setStyleSheet("color: #E0E0E0; font-size: 11px; border: none; background: transparent;");
    toastLay->addWidget(m_undoLabel, 1);
    m_undoBtn = new QPushButton(QStringLiteral("Undo"), m_undoToast);
    m_undoBtn->setCursor(Qt::PointingHandCursor);
    m_undoBtn->setStyleSheet(
        "QPushButton { background: #3B82F6; color: white; border: none; border-radius: 4px; padding: 4px 10px; font-weight: 600; }"
        "QPushButton:hover { background: #2563EB; }");
    toastLay->addWidget(m_undoBtn);
    m_undoToast->setVisible(false);
    listContainerLay->addWidget(m_undoToast);

    m_undoTimer = new QTimer(this);
    m_undoTimer->setSingleShot(true);
    m_undoTimer->setInterval(7000);
    connect(m_undoTimer, &QTimer::timeout, m_undoToast, &QWidget::hide);
    connect(m_undoBtn, &QPushButton::clicked, this, &MainWindow::onUndo);

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        if (m_countdownRemaining <= 0) {
            m_countdownTimer->stop();
            return;
        }
        --m_countdownRemaining;
        if (m_countdownRemaining > 0) {
            QString base = m_undoLabel->property("baseText").toString();
            if (!base.isEmpty()) {
                m_undoLabel->setText(base + QStringLiteral(" — Undo (%1s)").arg(m_countdownRemaining));
                m_statusLabel->setText(base + QStringLiteral(" — Undo available for %1s").arg(m_countdownRemaining));
            }
        } else {
            // Time up: hide toast and reset status to general
            m_undoToast->setVisible(false);
            m_countdownTimer->stop();
            m_statusLabel->setText(m_generalStatusText);
            m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
        }
    });

    m_statusResetTimer = new QTimer(this);
    m_statusResetTimer->setSingleShot(true);
    connect(m_statusResetTimer, &QTimer::timeout, this, [this]() {
        m_statusLabel->setText(m_generalStatusText);
        m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
    });

    root->addWidget(listContainer, 1);

    // Output directory selector — dark input, visible text
    auto *outRow = new QHBoxLayout();
    outRow->setSpacing(8);
    auto *outLabel = new QLabel(QStringLiteral("Output Folder:"), this);
    outLabel->setStyleSheet("font-weight: 600; color: #E2E8F0; font-size: 11px;");
    outLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    outRow->addWidget(outLabel);

    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setPlaceholderText(QStringLiteral("Choose output folder..."));
    m_outputEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_outputEdit->setMinimumHeight(28);
    m_outputEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #4A5568; border-radius: 6px; padding: 6px 10px; background: #2D2D2D; color: #E0E0E0; selection-background-color: #3B82F6; }"
        "QLineEdit:read-only { color: #E0E0E0; }");
    outRow->addWidget(m_outputEdit, 1);

    m_changeBtn = new QPushButton(QStringLiteral("Change..."), this);
    m_changeBtn->setToolTip(QStringLiteral("Select where the generated .txt files will be saved."));
    m_changeBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_changeBtn->setStyleSheet(
        "QPushButton { border: 1px solid #4A5568; border-radius: 6px; padding: 6px 14px; background: #3A3A3A; color: #E0E0E0; }"
        "QPushButton:hover { background: #4A5568; }");
    outRow->addWidget(m_changeBtn);
    root->addLayout(outRow);

    // Progress — dark track, blue chunk
    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(true);
    m_progress->setFormat(QStringLiteral("%v / %m files (%p%)"));
    m_progress->setVisible(false);
    m_progress->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_progress->setStyleSheet(
        "QProgressBar { border: 1px solid #4A5568; border-radius: 6px; background: #2D2D2D; color: #E0E0E0; text-align: center; padding: 2px; }"
        "QProgressBar::chunk { background: #3B82F6; border-radius: 4px; }");
    root->addWidget(m_progress);

    // Action area
    auto *actionRow = new QHBoxLayout();
    actionRow->addStretch();
    m_extractBtn = new QPushButton(QStringLiteral("Extract Text"), this);
    m_extractBtn->setMinimumHeight(36);
    m_extractBtn->setMinimumWidth(140);
    m_extractBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_extractBtn->setStyleSheet(
        "QPushButton { background: #2563EB; color: white; border: none; border-radius: 6px; font-weight: 700; padding: 6px 16px; }"
        "QPushButton:hover { background: #1D4ED8; }"
        "QPushButton:pressed { background: #1E40AF; }"
        "QPushButton:disabled { background: #4A5568; color: #A0AEC0; }");
    m_extractBtn->setToolTip(QStringLiteral("Start converting queued PDFs to .txt files."));
    actionRow->addWidget(m_extractBtn);
    root->addLayout(actionRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    root->addWidget(m_statusLabel);

    // Worker thread
    m_workerThread = new QThread(this);
    m_extractor = new PdfExtractor();
    m_extractor->moveToThread(m_workerThread);
    m_workerThread->start();

    // Settings
    loadSettings();
    updateEmptyPlaceholder();
    onSelectionChanged();

    // Connections
    connect(m_dropFrame, &DropFrame::filesDropped, this, &MainWindow::onDropFiles);
    connect(m_dropFrame, &DropFrame::browseRequested, this, &MainWindow::onBrowseClicked);
    connect(m_changeBtn, &QPushButton::clicked, this, &MainWindow::onChangeOutputDir);
    connect(m_extractBtn, &QPushButton::clicked, this, &MainWindow::onExtractClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::clearQueue);
    connect(m_removeSelectedBtn, &QPushButton::clicked, this, &MainWindow::removeSelected);
    connect(m_fileList, &QListWidget::itemSelectionChanged, this, &MainWindow::onSelectionChanged);

    // Cross-thread: MainWindow -> PdfExtractor
    connect(this, &MainWindow::destroyed, m_workerThread, &QThread::quit);

    // Extractor signals (queued)
    connect(m_extractor, &PdfExtractor::progressChanged, this, &MainWindow::onProgressChanged);
    connect(m_extractor, &PdfExtractor::fileSucceeded, this, &MainWindow::onFileSucceeded);
    connect(m_extractor, &PdfExtractor::fileFailed, this, &MainWindow::onFileFailed);
    connect(m_extractor, &PdfExtractor::finished, this, &MainWindow::onExtractionFinished);
}

MainWindow::~MainWindow() {
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(2000);
    }
    if (m_extractor) m_extractor->deleteLater();
}

void MainWindow::loadSettings() {
    QSettings settings(QStringLiteral("PDFToPlainText"), QStringLiteral("PDFToPlainText"));
    QString saved = settings.value(QStringLiteral("outputDir")).toString();
    if (saved.isEmpty() || !QDir(saved).exists()) {
        saved = defaultDocumentsPath();
    }
    m_outputDir = saved;
    m_outputEdit->setText(m_outputDir);
    m_outputEdit->setToolTip(m_outputDir);
}

void MainWindow::saveOutputDir(const QString &dir) {
    m_outputDir = dir;
    m_outputEdit->setText(dir);
    m_outputEdit->setToolTip(dir);
    QSettings settings(QStringLiteral("PDFToPlainText"), QStringLiteral("PDFToPlainText"));
    settings.setValue(QStringLiteral("outputDir"), dir);
}

QString MainWindow::defaultDocumentsPath() const {
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.isEmpty()) docs = QDir::homePath();
    return docs;
}

void MainWindow::updateEmptyPlaceholder() {
    bool empty = m_queuedFiles.isEmpty();
    if (m_listStack) m_listStack->setCurrentIndex(empty ? 1 : 0);
    if (m_fileList) m_fileList->setVisible(!empty);
}

void MainWindow::addFiles(const QStringList &paths) {
    int added = 0;
    for (const QString &p : paths) {
        if (m_queuedFiles.contains(p)) continue;
        QFileInfo fi(p);
        if (!fi.exists() || !fi.isFile()) continue;
        m_queuedFiles.append(p);
        auto *item = new QListWidgetItem(m_fileList);
        item->setData(Qt::UserRole, p);
        item->setSizeHint(QSize(0, 28));
        m_fileList->addItem(item);
        auto *row = new FileRowWidget(p, m_fileList);
        m_fileList->setItemWidget(item, row);
        connect(row, &FileRowWidget::removeRequested, this, [this, row]() {
            // Find row of this widget
            for (int i = 0; i < m_fileList->count(); ++i) {
                if (m_fileList->itemWidget(m_fileList->item(i)) == row) {
                    // Single remove with undo
                    s_undoState.paths = QStringList{ m_queuedFiles.at(i) };
                    s_undoState.rows = QList<int>{ i };
                    m_queuedFiles.removeAt(i);
                    delete m_fileList->takeItem(i);
                    updateEmptyPlaceholder();
                    onSelectionChanged();
                    m_extractBtn->setEnabled(!m_queuedFiles.isEmpty());
                    // Update general status to new queue size before countdown
                    m_generalStatusText = m_queuedFiles.isEmpty() ? QStringLiteral("Ready") : QStringLiteral("Queued %1 file(s). Ready to extract.").arg(m_queuedFiles.size());
                    updateEmptyPlaceholder();
                    onSelectionChanged();
                    startUndoCountdown(QStringLiteral("Removed 1 file"));
                    break;
                }
            }
        });
        ++added;
    }
    if (added > 0) {
        m_generalStatusText = QStringLiteral("Queued %1 file(s). Ready to extract.").arg(m_queuedFiles.size());
        m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
        m_statusLabel->setText(m_generalStatusText);
    } else if (m_queuedFiles.isEmpty()) {
        m_generalStatusText = QStringLiteral("Ready");
    }
    m_extractBtn->setEnabled(!m_queuedFiles.isEmpty());
    updateEmptyPlaceholder();
    onSelectionChanged();
}

void MainWindow::onBrowseClicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Select PDF files"), QDir::homePath(),
                                                      QStringLiteral("PDF Files (*.pdf)"));
    if (!files.isEmpty()) onDropFiles(files);
}

void MainWindow::onDropFiles(const QStringList &paths) {
    addFiles(paths);
}

void MainWindow::onChangeOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select output folder"), m_outputDir);
    if (!dir.isEmpty()) saveOutputDir(dir);
}

void MainWindow::onExtractClicked() {
    if (m_queuedFiles.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Nothing to convert"), QStringLiteral("Drop PDF files first."));
        return;
    }
    if (m_outputDir.isEmpty() || !QDir(m_outputDir).exists()) {
        QDir().mkpath(m_outputDir);
        if (!QDir(m_outputDir).exists()) {
            QMessageBox::warning(this, QStringLiteral("Output folder missing"), QStringLiteral("Please choose a valid output folder."));
            return;
        }
    }

    setExtracting(true);
    m_statusLabel->setStyleSheet("color: #60A5FA; font-size: 11px;");
    m_statusLabel->setText(QStringLiteral("Extracting %1 file(s)...").arg(m_queuedFiles.size()));

    const QStringList files = m_queuedFiles;
    const QString out = m_outputDir;
    QMetaObject::invokeMethod(m_extractor, [extractor = m_extractor, files, out]() {
        extractor->extract(files, out);
    }, Qt::QueuedConnection);
}

void MainWindow::setExtracting(bool extracting) {
    m_extractBtn->setEnabled(!extracting);
    m_clearBtn->setEnabled(!extracting);
    m_removeSelectedBtn->setEnabled(!extracting && m_fileList->selectedItems().size() > 0);
    m_changeBtn->setEnabled(!extracting);
    m_dropFrame->setEnabled(!extracting);
    m_progress->setVisible(extracting);
    if (extracting) {
        m_progress->setRange(0, m_queuedFiles.size());
        m_progress->setValue(0);
    }
}

void MainWindow::onProgressChanged(int completed, int total) {
    m_progress->setRange(0, total);
    m_progress->setValue(completed);
}

void MainWindow::onFileSucceeded(const QString &pdf, const QString &txt) {
    Q_UNUSED(pdf);
    Q_UNUSED(txt);
}

void MainWindow::onFileFailed(const QString &pdf, const QString &error) {
    m_statusLabel->setStyleSheet("color: #F87171; font-size: 11px;");
    m_statusLabel->setText(QStringLiteral("Failed: %1 — %2").arg(QFileInfo(pdf).fileName(), error));
}

void MainWindow::onExtractionFinished(int succeeded, int failed) {
    setExtracting(false);
    if (failed == 0) {
        m_statusLabel->setStyleSheet("color: #4ADE80; font-size: 11px; font-weight: 600;");
        m_statusLabel->setText(QStringLiteral("Converted %1 file(s) successfully.").arg(succeeded));
        // Store for undo before clearing
        s_undoState.paths = m_queuedFiles;
        QList<int> rows;
        for (int i = 0; i < s_undoState.paths.size(); ++i) rows.append(i);
        s_undoState.rows = rows;
        m_queuedFiles.clear();
        m_fileList->clear();
        m_extractBtn->setEnabled(false);
        m_generalStatusText = QStringLiteral("Ready");
        startUndoCountdown(QStringLiteral("Converted %1 file(s) — queue cleared").arg(succeeded));
    } else if (succeeded > 0) {
        m_statusLabel->setStyleSheet("color: #FBBF24; font-size: 11px; font-weight: 600;");
        m_statusLabel->setText(QStringLiteral("Converted %1 file(s), %2 failed. Check output folder: %3").arg(QString::number(succeeded), QString::number(failed), QDir::toNativeSeparators(m_outputDir)));
    } else {
        m_statusLabel->setStyleSheet("color: #F87171; font-size: 11px; font-weight: 600;");
        m_statusLabel->setText(QStringLiteral("Conversion failed for %1 file(s).").arg(failed));
    }
    updateEmptyPlaceholder();
    onSelectionChanged();
}

void MainWindow::clearQueue() {
    if (m_queuedFiles.isEmpty()) return;
    s_undoState.paths = m_queuedFiles;
    QList<int> rows;
    for (int i = 0; i < s_undoState.paths.size(); ++i) rows.append(i);
    s_undoState.rows = rows;
    m_queuedFiles.clear();
    m_fileList->clear();
    m_extractBtn->setEnabled(false);
    m_statusLabel->setText(QStringLiteral("Queue cleared."));
    m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
    m_progress->setVisible(false);
    updateEmptyPlaceholder();
    onSelectionChanged();
    m_generalStatusText = QStringLiteral("Ready");
    startUndoCountdown(QStringLiteral("Cleared %1 file(s)").arg(s_undoState.paths.size()));
}

void MainWindow::removeSelected() {
    auto selected = m_fileList->selectedItems();
    if (selected.isEmpty()) return;
    // Collect rows descending so removal doesn't shift indices
    QList<int> rows;
    for (auto *it : selected) rows.append(m_fileList->row(it));
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    s_undoState.paths.clear();
    s_undoState.rows.clear();
    for (int r : rows) {
        s_undoState.paths.prepend(m_queuedFiles.at(r));
        s_undoState.rows.prepend(r);
        m_queuedFiles.removeAt(r);
        delete m_fileList->takeItem(r);
    }
    m_generalStatusText = m_queuedFiles.isEmpty() ? QStringLiteral("Ready") : QStringLiteral("Queued %1 file(s). Ready to extract.").arg(m_queuedFiles.size());
    updateEmptyPlaceholder();
    onSelectionChanged();
    m_extractBtn->setEnabled(!m_queuedFiles.isEmpty());
    startUndoCountdown(QStringLiteral("Removed %1 file(s)").arg(rows.size()));
}

void MainWindow::onUndo() {
    m_undoTimer->stop();
    m_countdownTimer->stop();
    m_undoToast->setVisible(false);
    if (s_undoState.paths.isEmpty()) return;
    // Re-insert in ascending row order
    QList<QPair<int, QString>> pairs;
    for (int i = 0; i < s_undoState.paths.size(); ++i) pairs.append({s_undoState.rows[i], s_undoState.paths[i]});
    std::sort(pairs.begin(), pairs.end(), [](auto &a, auto &b){ return a.first < b.first; });
    for (auto &pr : pairs) {
        int r = qMin(pr.first, m_queuedFiles.size());
        m_queuedFiles.insert(r, pr.second);
        auto *item = new QListWidgetItem();
        item->setData(Qt::UserRole, pr.second);
        item->setSizeHint(QSize(0, 28));
        m_fileList->insertItem(r, item);
        auto *row = new FileRowWidget(pr.second, m_fileList);
        m_fileList->setItemWidget(item, row);
        connect(row, &FileRowWidget::removeRequested, this, [this, row]() {
            for (int i = 0; i < m_fileList->count(); ++i) {
                if (m_fileList->itemWidget(m_fileList->item(i)) == row) {
                    s_undoState.paths = QStringList{ m_queuedFiles.at(i) };
                    s_undoState.rows = QList<int>{ i };
                    m_queuedFiles.removeAt(i);
                    delete m_fileList->takeItem(i);
                    updateEmptyPlaceholder();
                    onSelectionChanged();
                    m_generalStatusText = m_queuedFiles.isEmpty() ? QStringLiteral("Ready") : QStringLiteral("Queued %1 file(s). Ready to extract.").arg(m_queuedFiles.size());
                    startUndoCountdown(QStringLiteral("Removed 1 file"));
                    m_extractBtn->setEnabled(!m_queuedFiles.isEmpty());
                    break;
                }
            }
        });
    }
    s_undoState.paths.clear();
    s_undoState.rows.clear();
    m_generalStatusText = m_queuedFiles.isEmpty() ? QStringLiteral("Ready") : QStringLiteral("Queued %1 file(s). Ready to extract.").arg(m_queuedFiles.size());
    updateEmptyPlaceholder();
    onSelectionChanged();
    m_extractBtn->setEnabled(!m_queuedFiles.isEmpty());
    startStatusReset(QStringLiteral("Undo successful"), 4000);
}

void MainWindow::onSelectionChanged() {
    bool hasSel = !m_fileList->selectedItems().isEmpty() && !m_queuedFiles.isEmpty();
    m_removeSelectedBtn->setEnabled(hasSel);
}

void MainWindow::startUndoCountdown(const QString &baseMsg) {
    // Save current general status if empty
    if (m_generalStatusText.isEmpty()) m_generalStatusText = QStringLiteral("Ready");
    m_undoLabel->setProperty("baseText", baseMsg);
    m_undoLabel->setText(baseMsg + QStringLiteral(" — Undo (7s)"));
    m_statusLabel->setText(baseMsg + QStringLiteral(" — Undo available for 7s"));
    m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
    m_undoToast->setVisible(true);
    m_countdownRemaining = 7;
    m_countdownTimer->start();
    m_undoTimer->start();
}

void MainWindow::startStatusReset(const QString &msg, int ms) {
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet("color: #4ADE80; font-size: 11px;");
    m_statusResetTimer->stop();
    // Disconnect previous singleShot connections to avoid multiple
    // Use singleShot timer to reset to general
    QTimer::singleShot(ms, this, [this]() {
        m_statusLabel->setText(m_generalStatusText);
        m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
    });
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    // QStackedWidget handles placeholder centering automatically — no manual geometry needed
}
