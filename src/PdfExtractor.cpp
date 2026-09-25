#include "PdfExtractor.h"

#include <memory>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>

#ifdef HAVE_POPPLER_QT6_CONFIG
#include <poppler-qt6.h>
#else
// Fallback include path for pkg-config builds
#include <poppler/qt6/poppler-qt6.h>
#endif

PdfExtractor::PdfExtractor(QObject *parent) : QObject(parent) {}

void PdfExtractor::extract(const QStringList &pdfPaths, const QString &outputDir) {
    int succeeded = 0;
    int failed = 0;
    const int total = pdfPaths.size();

    QDir outDir(outputDir);
    if (!outDir.exists()) {
        outDir.mkpath(".");
    }

    for (int i = 0; i < pdfPaths.size(); ++i) {
        const QString &pdfPath = pdfPaths.at(i);
        QFileInfo fi(pdfPath);
        const QString outPath = outDir.filePath(fi.completeBaseName() + ".txt");

        // Use unique_ptr for Poppler document
        std::unique_ptr<Poppler::Document> doc(Poppler::Document::load(pdfPath));
        if (!doc) {
            emit fileFailed(pdfPath, QStringLiteral("Failed to open PDF (encrypted or corrupted)."));
            ++failed;
            emit progressChanged(i + 1, total);
            continue;
        }
        if (doc->isLocked()) {
            emit fileFailed(pdfPath, QStringLiteral("PDF is password-protected."));
            ++failed;
            emit progressChanged(i + 1, total);
            continue;
        }
        // Poppler render hints not needed for text extraction
        doc->setRenderHint(Poppler::Document::Antialiasing, false);
        doc->setRenderHint(Poppler::Document::TextAntialiasing, false);

        QFile outFile(outPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit fileFailed(pdfPath, QStringLiteral("Cannot write output file: %1").arg(outFile.errorString()));
            ++failed;
            emit progressChanged(i + 1, total);
            continue;
        }

        QTextStream stream(&outFile);
        stream.setEncoding(QStringConverter::Utf8);

        const int pages = doc->numPages();
        for (int p = 0; p < pages; ++p) {
            std::unique_ptr<Poppler::Page> page(doc->page(p));
            if (!page) continue;
            // page->text() strips images/graphics and returns plain Unicode text.
            // Use PreserveLayout-like rect extraction for better reading order if available,
            // but default text() already yields raw string data as required.
            QString text = page->text(QRectF());
            if (!text.isEmpty()) {
                stream << text;
                // Ensure page separator; avoid runaway concatenation for scanned PDFs
                if (!text.endsWith('\n')) stream << '\n';
            }
        }

        outFile.close();
        ++succeeded;
        emit fileSucceeded(pdfPath, outPath);
        emit progressChanged(i + 1, total);
    }

    emit finished(succeeded, failed);
}
