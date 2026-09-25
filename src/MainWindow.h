#pragma once

#include <QMainWindow>
#include <QStringList>

class QListWidget;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QThread;
class DropFrame;
class PdfExtractor;

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
    QLabel *m_emptyPlaceholder = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QPushButton *m_changeBtn = nullptr;
    QPushButton *m_extractBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_statusLabel = nullptr;
    void updateEmptyPlaceholder();

    QStringList m_queuedFiles;
    QString m_outputDir;

    QThread *m_workerThread = nullptr;
    PdfExtractor *m_extractor = nullptr;
};
