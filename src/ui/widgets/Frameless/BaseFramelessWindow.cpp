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
#include <QApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScreen>
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
    setMouseTracking(true);
    setupUi();
    connectControls();
}

void BaseFramelessWindow::setupUi() {
    // Resize grip on right and bottom edges only - no margin on top/left
    m_rootLayout->setContentsMargins(0, 0, ResizeMargin, ResizeMargin);
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
    connect(m_titleBar, &TitleBar::mouseMoved, this, &BaseFramelessWindow::onTitleMouseMoved);
    connect(m_titleBar, &TitleBar::mouseReleased, this, &BaseFramelessWindow::onTitleMouseReleased);
    connect(m_titleBar, &TitleBar::mouseDoubleClicked, this, &BaseFramelessWindow::onTitleMouseDoubleClicked);
}

void BaseFramelessWindow::setWindowTitle(const QString &title) {
    QMainWindow::setWindowTitle(title);
    m_titleBar->setTitle(title);
}

// --- Edge resize helpers ---

bool BaseFramelessWindow::inCornerGrip(const QPoint &pos) const {
    // Triangular hit zone in the bottom-right corner.
    // The triangle spans from (w - GripSize, h) to (w, h - GripSize) to (w, h).
    // Point is inside if: dx + dy <= GripSize, where
    //   dx = distance from right edge, dy = distance from bottom edge
    int dx = width() - pos.x();
    int dy = height() - pos.y();
    return dx >= 0 && dy >= 0 && (dx + dy) <= GripSize;
}

int BaseFramelessWindow::edgesAt(const QPoint &pos) const {
    // Corner grip triangle takes priority - activates diagonal resize
    if (inCornerGrip(pos))
        return Right | Bottom;
    int edges = None;
    if (pos.x() >= width() - ResizeMargin)   edges |= Right;
    if (pos.y() >= height() - ResizeMargin)   edges |= Bottom;
    return edges;
}

Qt::CursorShape BaseFramelessWindow::cursorForEdges(int edges) const {
    if ((edges & (Bottom | Right)) == (Bottom | Right)) return Qt::SizeFDiagCursor;
    if (edges & Right)  return Qt::SizeHorCursor;
    if (edges & Bottom) return Qt::SizeVerCursor;
    return Qt::ArrowCursor;
}

// --- Mouse events for edge resize ---

void BaseFramelessWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !isMaximized()) {
        int edges = edgesAt(event->pos());
        if (edges != None) {
            m_resizing = true;
            m_resizeEdges = edges;
            m_resizeOrigin = event->globalPosition().toPoint();
            m_resizeGeom = geometry();
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void BaseFramelessWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_resizing) {
        QPoint delta = event->globalPosition().toPoint() - m_resizeOrigin;
        QRect g = m_resizeGeom;
        QSize minSz = minimumSize();
        if (minSz.isEmpty()) minSz = QSize(200, 150);

        if (m_resizeEdges & Right)
            g.setRight(g.right() + delta.x());
        if (m_resizeEdges & Bottom)
            g.setBottom(g.bottom() + delta.y());

        // Enforce minimum size
        if (g.width() < minSz.width())
            g.setWidth(minSz.width());
        if (g.height() < minSz.height())
            g.setHeight(minSz.height());

        setGeometry(g);
        event->accept();
        return;
    }

    // Update cursor when hovering near edges; unset when leaving the zone
    // so child widgets use their own cursor rather than inheriting a stale one
    if (!isMaximized()) {
        int edges = edgesAt(event->pos());
        if (edges != None) {
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
    if (m_resizing) {
        m_resizing = false;
        m_resizeEdges = None;
        unsetCursor();
        event->accept();
        return;
    }
    QMainWindow::mouseReleaseEvent(event);
}

// --- Title bar drag ---

void BaseFramelessWindow::onTitleMousePressed(const QPoint &globalPos) {
    if (isMaximized())
        return;
    m_dragging = true;
    m_dragOffset = globalPos - frameGeometry().topLeft();
}

void BaseFramelessWindow::onTitleMouseMoved(const QPoint &globalPos) {
    if (m_dragging && !isMaximized()) {
        QPoint newPos = globalPos - m_dragOffset;
        // Clamp so the title bar stays at least partially on screen
        if (QScreen *screen = QGuiApplication::screenAt(globalPos)) {
            QRect avail = screen->availableGeometry();
            newPos.setX(qBound(avail.left() - width() + 40, newPos.x(), avail.right() - 40));
            newPos.setY(qMax(avail.top(), newPos.y()));
        }
        move(newPos);
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

void BaseFramelessWindow::paintEvent(QPaintEvent *event) {
    QMainWindow::paintEvent(event);

    // Draw the corner grip indicator when not maximized/fullscreen
    if (isMaximized() || isFullScreen())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Three diagonal lines in the bottom-right corner
    QColor gripColor = palette().color(QPalette::Mid);
    gripColor.setAlpha(160);
    painter.setPen(QPen(gripColor, 1));

    int w = width();
    int h = height();
    // Lines at offsets 4, 8, 12 from the corner
    for (int off : {4, 8, 12}) {
        painter.drawLine(w - off, h - 1, w - 1, h - off);
    }
}

void BaseFramelessWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateResizeBorder();
    }
}

void BaseFramelessWindow::updateResizeBorder() {
    // No resize border when maximized or fullscreen - reclaim the space
    if (isMaximized() || isFullScreen()) {
        m_rootLayout->setContentsMargins(0, 0, 0, 0);
    } else {
        m_rootLayout->setContentsMargins(0, 0, ResizeMargin, ResizeMargin);
    }
}

} // namespace ui::widgets
