#include "DropFrame.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QUrl>

DropFrame::DropFrame(QWidget *parent) : QFrame(parent) {
    setAcceptDrops(true);
    setFrameShape(QFrame::StyledPanel);
    setCursor(Qt::PointingHandCursor);
    setObjectName("DropFrame");
    // Dark cohesive — charcoal, high-contrast dashed border, hover bright
    setStyleSheet(
        "QFrame#DropFrame {"
        "  border: 2px dashed #4A5568;"
        "  border-radius: 10px;"
        "  background: #2D2D2D;"
        "}"
        "QFrame#DropFrame[dragHover=\"true\"] {"
        "  border-color: #3B82F6;"
        "  background: #252525;"
        "}"
    );
}

bool DropFrame::isPdfFile(const QString &path) {
    return path.endsWith(".pdf", Qt::CaseInsensitive);
}

void DropFrame::dragEnterEvent(QDragEnterEvent *event) {
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    // Accept only if at least one PDF is present
    bool hasPdf = false;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile() && isPdfFile(url.toLocalFile())) {
            hasPdf = true;
            break;
        }
    }
    if (hasPdf) {
        m_dragHover = true;
        setProperty("dragHover", true);
        style()->unpolish(this);
        style()->polish(this);
        update();
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropFrame::dragMoveEvent(QDragMoveEvent *event) {
    event->acceptProposedAction();
}

void DropFrame::dropEvent(QDropEvent *event) {
    m_dragHover = false;
    setProperty("dragHover", false);
    style()->unpolish(this);
    style()->polish(this);
    update();

    QStringList pdfs;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        const QString path = url.toLocalFile();
        if (isPdfFile(path)) pdfs.append(path);
    }
    if (!pdfs.isEmpty()) {
        emit filesDropped(pdfs);
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropFrame::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit browseRequested();
    }
    QFrame::mousePressEvent(event);
}

void DropFrame::paintEvent(QPaintEvent *event) {
    QFrame::paintEvent(event);
    // Ensure stylesheet border is painted correctly with rounded corners
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
