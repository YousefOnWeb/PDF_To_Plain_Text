#pragma once

#include <QFrame>

class DropFrame : public QFrame {
    Q_OBJECT
public:
    explicit DropFrame(QWidget *parent = nullptr);

signals:
    void filesDropped(const QStringList &filePaths);
    void browseRequested();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_dragHover = false;
    static bool isPdfFile(const QString &path);
};
