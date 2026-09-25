#include "MainWindow.h"
#include "DropFrame.h"
#include "PdfExtractor.h"

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

    // File list — dark cohesive, responsive via Expanding + ScrollBar
    auto *listHeader = new QHBoxLayout();
    auto *queueTitle = new QLabel(QStringLiteral("Queued files:"), this);
    queueTitle->setStyleSheet("font-weight: 600; color: #E2E8F0; font-size: 11px;");
    listHeader->addWidget(queueTitle);
    listHeader->addStretch();
    m_clearBtn = new QPushButton(QStringLiteral("Clear"), this);
    m_clearBtn->setToolTip(QStringLiteral("Remove all files from the queue."));
    m_clearBtn->setFlat(true);
    m_clearBtn->setStyleSheet("QPushButton { color: #F87171; font-size: 11px; border: none; } QPushButton:hover { color: #EF4444; }");
    listHeader->addWidget(m_clearBtn);
    root->addLayout(listHeader);

    // Container for list + empty placeholder — ensures proper flex constraints, no overlap
    auto *listContainer = new QWidget(this);
    listContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *listStack = new QVBoxLayout(listContainer);
    listStack->setContentsMargins(0, 0, 0, 0);
    listStack->setSpacing(0);

    m_fileList = new QListWidget(listContainer);
    m_fileList->setAlternatingRowColors(false);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_fileList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_fileList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_fileList->setMinimumHeight(90);
    m_fileList->setStyleSheet(
        "QListWidget { border: 1px solid #4A5568; border-radius: 6px; background: #2D2D2D; color: #E0E0E0; }"
        "QListWidget::item { padding: 6px 8px; border: none; color: #E0E0E0; }"
        "QListWidget::item:selected { background: #3B82F6; color: white; }"
        "QListWidget::item:hover { background: #3A3A3A; }");
    listStack->addWidget(m_fileList);

    root->addWidget(listContainer, 1);

    m_emptyPlaceholder = new QLabel(QStringLiteral("No files queued. Drag files above to begin."), m_fileList->viewport());
    m_emptyPlaceholder->setAlignment(Qt::AlignCenter);
    m_emptyPlaceholder->setWordWrap(true);
    m_emptyPlaceholder->setStyleSheet("color: #A0AEC0; font-size: 11px; padding: 18px; background: transparent; border: none;");
    m_emptyPlaceholder->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyPlaceholder->setGeometry(m_fileList->viewport()->rect());
    m_emptyPlaceholder->show();

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

    // Connections
    connect(m_dropFrame, &DropFrame::filesDropped, this, &MainWindow::onDropFiles);
    connect(m_dropFrame, &DropFrame::browseRequested, this, &MainWindow::onBrowseClicked);
    connect(m_changeBtn, &QPushButton::clicked, this, &MainWindow::onChangeOutputDir);
    connect(m_extractBtn, &QPushButton::clicked, this, &MainWindow::onExtractClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::clearQueue);

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
    if (m_emptyPlaceholder) {
        m_emptyPlaceholder->setVisible(empty);
        if (empty && m_fileList && m_fileList->viewport()) {
            m_emptyPlaceholder->setGeometry(m_fileList->viewport()->rect());
            m_emptyPlaceholder->raise();
        }
    }
}

void MainWindow::addFiles(const QStringList &paths) {
    int added = 0;
    for (const QString &p : paths) {
        if (m_queuedFiles.contains(p)) continue;
        QFileInfo fi(p);
        if (!fi.exists() || !fi.isFile()) continue;
        m_queuedFiles.append(p);
        m_fileList->addItem(fi.fileName() + QStringLiteral("  —  ") + QDir::toNativeSeparators(p));
        ++added;
    }
    if (added > 0) {
        m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
        m_statusLabel->setText(QStringLiteral("Queued %1 file(s). Ready to extract.").arg(m_queuedFiles.size()));
    }
    m_extractBtn->setEnabled(!m_queuedFiles.isEmpty());
    updateEmptyPlaceholder();
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
        m_queuedFiles.clear();
        m_fileList->clear();
        m_extractBtn->setEnabled(false);
    } else if (succeeded > 0) {
        m_statusLabel->setStyleSheet("color: #FBBF24; font-size: 11px; font-weight: 600;");
        m_statusLabel->setText(QStringLiteral("Converted %1 file(s), %2 failed. Check output folder: %3").arg(QString::number(succeeded), QString::number(failed), QDir::toNativeSeparators(m_outputDir)));
    } else {
        m_statusLabel->setStyleSheet("color: #F87171; font-size: 11px; font-weight: 600;");
        m_statusLabel->setText(QStringLiteral("Conversion failed for %1 file(s).").arg(failed));
    }
    updateEmptyPlaceholder();
}

void MainWindow::clearQueue() {
    m_queuedFiles.clear();
    m_fileList->clear();
    m_extractBtn->setEnabled(false);
    m_statusLabel->setText(QStringLiteral("Queue cleared."));
    m_statusLabel->setStyleSheet("color: #A0AEC0; font-size: 11px;");
    m_progress->setVisible(false);
    updateEmptyPlaceholder();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateEmptyPlaceholder();
}
