#pragma once
#include <QWidget>

class QLabel;
class QToolButton;

class FileRowWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileRowWidget(const QString &filePath, QWidget *parent = nullptr);
    QString filePath() const { return m_filePath; }

signals:
    void removeRequested(FileRowWidget *self);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_filePath;
    QLabel *m_label = nullptr;
    QToolButton *m_removeBtn = nullptr;
};
