#pragma once

#include <QObject>
#include <QStringList>

// Worker that runs off the GUI thread.
// Emits progress after each file and a final summary.
class PdfExtractor : public QObject {
    Q_OBJECT
public:
    explicit PdfExtractor(QObject *parent = nullptr);

public slots:
    void extract(const QStringList &pdfPaths, const QString &outputDir);

signals:
    void progressChanged(int completed, int total);
    void fileSucceeded(const QString &pdfPath, const QString &txtPath);
    void fileFailed(const QString &pdfPath, const QString &error);
    void finished(int succeeded, int failed);
};
