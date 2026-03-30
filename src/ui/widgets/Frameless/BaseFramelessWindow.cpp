// 5250ng - A modern IBM TN5250 terminal emulator
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "BaseFramelessWindow.h"
#include <QMouseEvent>
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
    setMouseTracking(true);
    setupUi();
    connectControls();
}

void BaseFramelessWindow::setupUi() {
    m_rootLayout->setContentsMargins(ResizeMargin, ResizeMargin, ResizeMargin, ResizeMargin);
    m_rootLayout->setSpacing(0);
    m_rootLayout->addWidget(m_titleBar, 0);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    m_rootLayout->addWidget(m_content, 1);
    m_central->setLayout(m_rootLayout);
    m_central->setMouseTracking(true);
    setCentralWidget(m_central);
}

void BaseFramelessWindow::connectControls() {
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &BaseFramelessWindow::onMinimize);
    connect(m_titleBar, &TitleBar::maximizeRestoreRequested, this, &BaseFramelessWindow::onMaximizeRestore);
    connect(m_titleBar, &TitleBar::closeRequested, this, &BaseFramelessWindow::onClose);
    connect(m_titleBar, &TitleBar::mousePressed, this, &BaseFramelessWindow::onTitleMousePressed);
    connect(m_titleBar, &TitleBar::mouseDoubleClicked, this, &BaseFramelessWindow::onTitleMouseDoubleClicked);
}

void BaseFramelessWindow::setWindowTitle(const QString &title) {
    QMainWindow::setWindowTitle(title);
    m_titleBar->setTitle(title);
}

// --- Edge detection for all 4 edges and 4 corners ---

Qt::Edges BaseFramelessWindow::edgesAt(const QPoint &pos) const {
    Qt::Edges edges;
    if (pos.x() < ResizeMargin)                edges |= Qt::LeftEdge;
    if (pos.x() >= width() - ResizeMargin)     edges |= Qt::RightEdge;
    if (pos.y() < ResizeMargin)                edges |= Qt::TopEdge;
    if (pos.y() >= height() - ResizeMargin)    edges |= Qt::BottomEdge;
    return edges;
}

Qt::CursorShape BaseFramelessWindow::cursorForEdges(Qt::Edges edges) const {
    if (edges == (Qt::TopEdge | Qt::LeftEdge))     return Qt::SizeFDiagCursor;
    if (edges == (Qt::BottomEdge | Qt::RightEdge)) return Qt::SizeFDiagCursor;
    if (edges == (Qt::TopEdge | Qt::RightEdge))    return Qt::SizeBDiagCursor;
    if (edges == (Qt::BottomEdge | Qt::LeftEdge))  return Qt::SizeBDiagCursor;
    if (edges & (Qt::LeftEdge | Qt::RightEdge))    return Qt::SizeHorCursor;
    if (edges & (Qt::TopEdge | Qt::BottomEdge))    return Qt::SizeVerCursor;
    return Qt::ArrowCursor;
}

// --- Mouse events: delegate to system move/resize ---

void BaseFramelessWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !isMaximized() && !isFullScreen()) {
        Qt::Edges edges = edgesAt(event->pos());
        if (edges && windowHandle()) {
            windowHandle()->startSystemResize(edges);
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void BaseFramelessWindow::mouseMoveEvent(QMouseEvent *event) {
    // Fallback drag handling
    if (m_fallbackDragging) {
        move(event->globalPosition().toPoint() - m_fallbackDragOffset);
        event->accept();
        return;
    }
    // Update cursor shape when hovering near edges
    if (!isMaximized() && !isFullScreen()) {
        Qt::Edges edges = edgesAt(event->pos());
        if (edges) {
            setCursor(cursorForEdges(edges));
        } else {
            unsetCursor();
        }
    } else {
        unsetCursor();
    }
    QMainWindow::mouseMoveEvent(event);
}

void BaseFramelessWindow::mouseReleaseEvent(QMouseEvent *event) {
    m_fallbackDragging = false;
    unsetCursor();
    QMainWindow::mouseReleaseEvent(event);
}

// --- Title bar interactions ---

void BaseFramelessWindow::onTitleMousePressed(const QPoint &globalPos) {
    if (windowHandle() && !windowHandle()->startSystemMove()) {
        // Fallback for platforms that don't support system move (e.g. some Wayland compositors)
        m_fallbackDragging = true;
        m_fallbackDragOffset = globalPos - frameGeometry().topLeft();
    }
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

void BaseFramelessWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateResizeBorder();
    }
}

void BaseFramelessWindow::updateResizeBorder() {
    if (isMaximized() || isFullScreen()) {
        m_rootLayout->setContentsMargins(0, 0, 0, 0);
    } else {
        m_rootLayout->setContentsMargins(ResizeMargin, ResizeMargin, ResizeMargin, ResizeMargin);
    }
}

} // namespace ui::widgets
