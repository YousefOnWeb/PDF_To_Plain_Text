#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PDF To Plain Text"));
    QApplication::setOrganizationName(QStringLiteral("PDFToPlainText"));
    QApplication::setOrganizationDomain(QStringLiteral("pdf-to-plain-text.local"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    MainWindow w;
    w.show();
    return app.exec();
}
