#pragma once

#include <QMainWindow>
#include <QStringList>

class QListWidget;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QThread;
class QStackedWidget;
class QToolButton;
class DropFrame;
class PdfExtractor;
class FileRowWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onBrowseClicked();
    void onDropFiles(const QStringList &paths);
    void onChangeOutputDir();
    void onExtractClicked();
    void onProgressChanged(int completed, int total);
    void onFileSucceeded(const QString &pdf, const QString &txt);
    void onFileFailed(const QString &pdf, const QString &error);
    void onExtractionFinished(int succeeded, int failed);
    void clearQueue();
    void removeSelected();
    void onUndo();
    void onSelectionChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void loadSettings();
    void saveOutputDir(const QString &dir);
    void addFiles(const QStringList &paths);
    void setExtracting(bool extracting);
    QString defaultDocumentsPath() const;

    DropFrame *m_dropFrame = nullptr;
    QLabel *m_dropLabel = nullptr;
    QListWidget *m_fileList = nullptr;
    QStackedWidget *m_listStack = nullptr;
    QLabel *m_emptyPlaceholder = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QPushButton *m_changeBtn = nullptr;
    QPushButton *m_extractBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_removeSelectedBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_undoToast = nullptr;
    QLabel *m_undoLabel = nullptr;
    QPushButton *m_undoBtn = nullptr;
    class QTimer *m_undoTimer = nullptr;
    class QTimer *m_countdownTimer = nullptr;
    class QTimer *m_statusResetTimer = nullptr;
    int m_countdownRemaining = 0;
    QString m_generalStatusText;
    void updateEmptyPlaceholder();
    void startUndoCountdown(const QString &baseMsg);
    void startStatusReset(const QString &msg, int ms = 4000);

    // Global failure summary
    QToolButton *m_globalErrorBtn = nullptr;
    QWidget *m_globalPopover = nullptr;
    QMap<QString, QString> m_failedErrors;
    QWidget *m_copyToast = nullptr;
    QLabel *m_copyLabel = nullptr;
    QTimer *m_copyTimer = nullptr;
    void updateGlobalErrorButton();
    void showGlobalPopover();
    void showCopyToast(const QString &msg);

    QStringList m_queuedFiles;
    QString m_outputDir;

    QThread *m_workerThread = nullptr;
    PdfExtractor *m_extractor = nullptr;
};
