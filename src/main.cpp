#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PDF To Plain Text"));
    QApplication::setOrganizationName(QStringLiteral("PDFToPlainText"));
    QApplication::setOrganizationDomain(QStringLiteral("pdf-to-plain-text.local"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    // Cohesive dark theme — Fusion + dark palette for WCAG contrast
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(0x1E, 0x1E, 0x1E));
    dark.setColor(QPalette::WindowText, QColor(0xE0, 0xE0, 0xE0));
    dark.setColor(QPalette::Base, QColor(0x2D, 0x2D, 0x2D));
    dark.setColor(QPalette::AlternateBase, QColor(0x25, 0x25, 0x25));
    dark.setColor(QPalette::ToolTipBase, QColor(0x2D, 0x2D, 0x2D));
    dark.setColor(QPalette::ToolTipText, QColor(0xE0, 0xE0, 0xE0));
    dark.setColor(QPalette::Text, QColor(0xE0, 0xE0, 0xE0));
    dark.setColor(QPalette::Button, QColor(0x2D, 0x2D, 0x2D));
    dark.setColor(QPalette::ButtonText, QColor(0xE0, 0xE0, 0xE0));
    dark.setColor(QPalette::BrightText, Qt::red);
    dark.setColor(QPalette::Highlight, QColor(0x3B, 0x82, 0xF6));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(dark);

    MainWindow w;
    w.show();
    return app.exec();
}
