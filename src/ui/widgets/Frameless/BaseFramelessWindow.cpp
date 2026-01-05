#include "BaseFramelessWindow.h"
#include <QApplication>
#include <QMouseEvent>
#include <QStyle>
#include <QWindow>

namespace ui::widgets {

BaseFramelessWindow::BaseFramelessWindow(QWidget *parent)
    : QMainWindow(parent),
      m_central(new QWidget(this)),
      m_titleBar(new TitleBar(m_central)),
      m_content(new QWidget(m_central)),
      m_rootLayout(new QVBoxLayout(m_central)),
      m_contentLayout(new QVBoxLayout(m_content)) {
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setupUi();
    connectControls();
}

void BaseFramelessWindow::setupUi() {
    // Root central widget layout
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);
    m_rootLayout->addWidget(m_titleBar, 0);
    // 1px separator under title bar via stylesheet; keep content flush
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    m_rootLayout->addWidget(m_content, 1);
    m_central->setLayout(m_rootLayout);
    setCentralWidget(m_central);
}

void BaseFramelessWindow::connectControls() {
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &BaseFramelessWindow::onMinimize);
    connect(m_titleBar, &TitleBar::maximizeRestoreRequested, this, &BaseFramelessWindow::onMaximizeRestore);
    connect(m_titleBar, &TitleBar::closeRequested, this, &BaseFramelessWindow::onClose);
    connect(m_titleBar, &TitleBar::mousePressed, this, &BaseFramelessWindow::onTitleMousePressed);
    connect(m_titleBar, &TitleBar::mouseMoved, this, &BaseFramelessWindow::onTitleMouseMoved);
    connect(m_titleBar, &TitleBar::mouseReleased, this, &BaseFramelessWindow::onTitleMouseReleased);
    connect(m_titleBar, &TitleBar::mouseDoubleClicked, this, &BaseFramelessWindow::onTitleMouseDoubleClicked);
}

void BaseFramelessWindow::onTitleMousePressed(const QPoint &globalPos) {
    if (isMaximized())
        return;
    m_dragging = true;
    m_dragOffset = globalPos - frameGeometry().topLeft();
}

void BaseFramelessWindow::onTitleMouseMoved(const QPoint &globalPos) {
    if (m_dragging && !isMaximized()) {
        move(globalPos - m_dragOffset);
    }
}

void BaseFramelessWindow::onTitleMouseReleased() {
    m_dragging = false;
}

void BaseFramelessWindow::onTitleMouseDoubleClicked(const QPoint &) {
    onMaximizeRestore();
}

void BaseFramelessWindow::onMinimize() { showMinimized(); }

void BaseFramelessWindow::onMaximizeRestore() {
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void BaseFramelessWindow::onClose() { close(); }

} // namespace ui::widgets


