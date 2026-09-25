#include "FileRowWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QFileInfo>
#include <QDir>
#include <QMouseEvent>
#include <QListWidget>
#include <QApplication>
#include <QClipboard>
#include <QTimer>

FileRowWidget::FileRowWidget(const QString &filePath, QWidget *parent)
    : QWidget(parent), m_filePath(filePath) {
    setObjectName("FileRow");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(6);

    QFileInfo fi(filePath);
    QString text = fi.fileName() + QStringLiteral("  —  ") + QDir::toNativeSeparators(filePath);
    m_label = new QLabel(text, this);
    m_label->setStyleSheet("color: #E0E0E0; background: transparent; border: none;");
    m_label->setTextInteractionFlags(Qt::NoTextInteraction);
    m_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_label->setToolTip(filePath);
    lay->addWidget(m_label, 1);

    m_errorBtn = new QToolButton(this);
    m_errorBtn->setText(QStringLiteral("⚠"));
    m_errorBtn->setToolTip(QStringLiteral(""));
    m_errorBtn->setCursor(Qt::PointingHandCursor);
    m_errorBtn->setFixedSize(18, 18);
    m_errorBtn->setStyleSheet(
        "QToolButton { border: none; border-radius: 3px; color: #F87171; background: rgba(248,113,113,0.15); font-size: 11px; }"
        "QToolButton:hover { background: rgba(248,113,113,0.25); }");
    m_errorBtn->setVisible(false);
    lay->addWidget(m_errorBtn);

    m_removeBtn = new QToolButton(this);
    m_removeBtn->setText(QStringLiteral("×"));
    m_removeBtn->setToolTip(QStringLiteral("Remove this file"));
    m_removeBtn->setCursor(Qt::PointingHandCursor);
    m_removeBtn->setFixedSize(18, 18);
    m_removeBtn->setStyleSheet(
        "QToolButton { border: none; border-radius: 3px; color: #A0AEC0; background: transparent; font-size: 13px; font-weight: 700; }"
        "QToolButton:hover { color: #F87171; background: #3A3A3A; }"
        "QToolButton:pressed { background: #4A5568; }");
    m_removeBtn->setVisible(false);
    lay->addWidget(m_removeBtn);

    connect(m_removeBtn, &QToolButton::clicked, this, [this]() { emit removeRequested(this); });
    connect(m_errorBtn, &QToolButton::clicked, this, [this]() {
        if (m_error.isEmpty()) return;
        QGuiApplication::clipboard()->setText(m_error);
        // Brief copied feedback via tooltip
        m_errorBtn->setToolTip(QStringLiteral("Error copied"));
        QTimer::singleShot(1500, this, [this]() { m_errorBtn->setToolTip(m_error); });
        // Also show parent window's copied toast if available
        if (auto *win = window()) {
            if (auto *toast = win->findChild<QWidget*>("CopyToast")) {
                if (auto *lbl = toast->findChild<QLabel*>()) lbl->setText(QStringLiteral("Error copied"));
                toast->setVisible(true);
                QTimer::singleShot(2000, toast, &QWidget::hide);
            }
        }
    });

    setStyleSheet(
        "QWidget#FileRow { background: transparent; }"
        "QWidget#FileRow:hover { background: #3A3A3A; border-radius: 4px; }");
}

void FileRowWidget::enterEvent(QEnterEvent *event) {
    m_removeBtn->setVisible(true);
    QWidget::enterEvent(event);
}

void FileRowWidget::leaveEvent(QEvent *event) {
    m_removeBtn->setVisible(false);
    QWidget::leaveEvent(event);
}

void FileRowWidget::setFailed(bool failed, const QString &error) {
    m_failed = failed;
    m_error = error;
    m_errorBtn->setVisible(failed);
    m_errorBtn->setToolTip(failed ? error : QString());
    if (failed) {
        m_label->setStyleSheet("color: #FCA5A5; background: transparent; border: none;");
        setStyleSheet(
            "QWidget#FileRow { background: rgba(248,113,113,0.08); border-radius: 4px; }"
            "QWidget#FileRow:hover { background: rgba(248,113,113,0.15); border-radius: 4px; }");
    } else {
        m_label->setStyleSheet("color: #E0E0E0; background: transparent; border: none;");
        setStyleSheet(
            "QWidget#FileRow { background: transparent; }"
            "QWidget#FileRow:hover { background: #3A3A3A; border-radius: 4px; }");
    }
}

void FileRowWidget::mousePressEvent(QMouseEvent *event) {
    // Let buttons handle their own clicks
    if (m_removeBtn->geometry().contains(event->pos()) || m_errorBtn->geometry().contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }
    // Propagate selection to the QListWidget item that hosts this widget
    QWidget *vp = parentWidget();
    QListWidget *list = qobject_cast<QListWidget*>(vp ? vp->parentWidget() : nullptr);
    if (!list) list = qobject_cast<QListWidget*>(vp);
    if (list) {
        for (int i = 0; i < list->count(); ++i) {
            if (list->itemWidget(list->item(i)) == this) {
                QListWidgetItem *it = list->item(i);
                Qt::KeyboardModifiers mods = event->modifiers();
                if (mods & Qt::ControlModifier) {
                    bool sel = it->isSelected();
                    it->setSelected(!sel);
                    list->setCurrentItem(it);
                } else if (mods & Qt::ShiftModifier) {
                    int cur = list->currentRow();
                    if (cur >= 0) {
                        int a = qMin(cur, i), b = qMax(cur, i);
                        list->clearSelection();
                        for (int r = a; r <= b; ++r) list->item(r)->setSelected(true);
                        list->setCurrentItem(it);
                    } else {
                        it->setSelected(true);
                        list->setCurrentItem(it);
                    }
                } else {
                    list->clearSelection();
                    it->setSelected(true);
                    list->setCurrentItem(it);
                }
                break;
            }
        }
    }
    QWidget::mousePressEvent(event);
}
