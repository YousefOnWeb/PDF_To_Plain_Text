#pragma once
#include <QWidget>

class QLabel;
class QToolButton;

class FileRowWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileRowWidget(const QString &filePath, QWidget *parent = nullptr);
    QString filePath() const { return m_filePath; }
    void setFailed(bool failed, const QString &error = QString());
    bool isFailed() const { return m_failed; }
    QString errorText() const { return m_error; }

signals:
    void removeRequested(FileRowWidget *self);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString m_filePath;
    bool m_failed = false;
    QString m_error;
    QLabel *m_label = nullptr;
    QToolButton *m_errorBtn = nullptr;
    QToolButton *m_removeBtn = nullptr;
};
