#include "FileRowWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QFileInfo>
#include <QDir>

FileRowWidget::FileRowWidget(const QString &filePath, QWidget *parent)
    : QWidget(parent), m_filePath(filePath) {
    setObjectName("FileRow");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(8);

    QFileInfo fi(filePath);
    QString text = fi.fileName() + QStringLiteral("  —  ") + QDir::toNativeSeparators(filePath);
    m_label = new QLabel(text, this);
    m_label->setStyleSheet("color: #E0E0E0; background: transparent; border: none;");
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_label->setToolTip(filePath);
    lay->addWidget(m_label, 1);

    m_removeBtn = new QToolButton(this);
    m_removeBtn->setText(QStringLiteral("×"));
    m_removeBtn->setToolTip(QStringLiteral("Remove this file"));
    m_removeBtn->setCursor(Qt::PointingHandCursor);
    m_removeBtn->setFixedSize(20, 20);
    m_removeBtn->setStyleSheet(
        "QToolButton { border: none; border-radius: 4px; color: #A0AEC0; background: transparent; font-size: 14px; font-weight: 700; }"
        "QToolButton:hover { color: #F87171; background: #3A3A3A; }"
        "QToolButton:pressed { background: #4A5568; }");
    m_removeBtn->setVisible(false);
    lay->addWidget(m_removeBtn);

    connect(m_removeBtn, &QToolButton::clicked, this, [this]() { emit removeRequested(this); });

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
