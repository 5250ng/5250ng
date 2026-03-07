#include "BaseFramelessDialog.h"

namespace ui::widgets {

BaseFramelessDialog::BaseFramelessDialog(QWidget *parent)
    : QDialog(parent),
      m_titleBar(new TitleBar(this)),
      m_contentLayout(new QVBoxLayout()) {
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

    // Dialogs don't need minimize/maximize
    m_titleBar->setMinMaxVisible(false);
    // Menu bar is empty for dialogs but keeps the layout consistent
    m_titleBar->menuBar()->hide();

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_titleBar, 0);

    QWidget *content = new QWidget(this);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    content->setLayout(m_contentLayout);
    root->addWidget(content, 1);

    setLayout(root);

    // Wire title bar close to dialog reject
    connect(m_titleBar, &TitleBar::closeRequested, this, &QDialog::reject);
    // Wire drag
    connect(m_titleBar, &TitleBar::mousePressed, this, &BaseFramelessDialog::onTitleMousePressed);
    connect(m_titleBar, &TitleBar::mouseMoved, this, &BaseFramelessDialog::onTitleMouseMoved);
    connect(m_titleBar, &TitleBar::mouseReleased, this, &BaseFramelessDialog::onTitleMouseReleased);
}

void BaseFramelessDialog::setWindowTitle(const QString &title) {
    QDialog::setWindowTitle(title);
    m_titleBar->setTitle(title);
}

void BaseFramelessDialog::onTitleMousePressed(const QPoint &globalPos) {
    m_dragging = true;
    m_dragOffset = globalPos - frameGeometry().topLeft();
}

void BaseFramelessDialog::onTitleMouseMoved(const QPoint &globalPos) {
    if (m_dragging) {
        move(globalPos - m_dragOffset);
    }
}

void BaseFramelessDialog::onTitleMouseReleased() {
    m_dragging = false;
}

} // namespace ui::widgets
