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
    setMouseTracking(true);
    setupUi();
    connectControls();
}

void BaseFramelessWindow::setupUi() {
    // Leave a resize grip border around the content so mouse events
    // reach the window (not swallowed by child widgets).
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
    connect(m_titleBar, &TitleBar::mouseMoved, this, &BaseFramelessWindow::onTitleMouseMoved);
    connect(m_titleBar, &TitleBar::mouseReleased, this, &BaseFramelessWindow::onTitleMouseReleased);
    connect(m_titleBar, &TitleBar::mouseDoubleClicked, this, &BaseFramelessWindow::onTitleMouseDoubleClicked);
}

void BaseFramelessWindow::setWindowTitle(const QString &title) {
    QMainWindow::setWindowTitle(title);
    m_titleBar->setTitle(title);
}

// --- Edge resize helpers ---

int BaseFramelessWindow::edgesAt(const QPoint &pos) const {
    int edges = None;
    if (pos.x() <= ResizeMargin)                edges |= Left;
    if (pos.x() >= width() - ResizeMargin)      edges |= Right;
    if (pos.y() <= ResizeMargin)                 edges |= Top;
    if (pos.y() >= height() - ResizeMargin)      edges |= Bottom;
    return edges;
}

Qt::CursorShape BaseFramelessWindow::cursorForEdges(int edges) const {
    if ((edges & (Top | Left)) == (Top | Left))       return Qt::SizeFDiagCursor;
    if ((edges & (Bottom | Right)) == (Bottom | Right)) return Qt::SizeFDiagCursor;
    if ((edges & (Top | Right)) == (Top | Right))     return Qt::SizeBDiagCursor;
    if ((edges & (Bottom | Left)) == (Bottom | Left)) return Qt::SizeBDiagCursor;
    if (edges & Left)   return Qt::SizeHorCursor;
    if (edges & Right)  return Qt::SizeHorCursor;
    if (edges & Top)    return Qt::SizeVerCursor;
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
        if (m_resizeEdges & Left) {
            int newLeft = g.left() + delta.x();
            if (g.right() - newLeft + 1 >= minSz.width())
                g.setLeft(newLeft);
        }
        if (m_resizeEdges & Top) {
            int newTop = g.top() + delta.y();
            if (g.bottom() - newTop + 1 >= minSz.height())
                g.setTop(newTop);
        }

        // Enforce minimum size
        if (g.width() < minSz.width())
            g.setWidth(minSz.width());
        if (g.height() < minSz.height())
            g.setHeight(minSz.height());

        setGeometry(g);
        event->accept();
        return;
    }

    // Update cursor when hovering near edges
    if (!isMaximized()) {
        int edges = edgesAt(event->pos());
        setCursor(cursorForEdges(edges));
    } else {
        setCursor(Qt::ArrowCursor);
    }

    QMainWindow::mouseMoveEvent(event);
}

void BaseFramelessWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (m_resizing) {
        m_resizing = false;
        m_resizeEdges = None;
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

void BaseFramelessWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateResizeBorder();
    }
}

void BaseFramelessWindow::updateResizeBorder() {
    // No resize border when maximized or fullscreen — reclaim the space
    if (isMaximized() || isFullScreen()) {
        m_rootLayout->setContentsMargins(0, 0, 0, 0);
    } else {
        m_rootLayout->setContentsMargins(ResizeMargin, ResizeMargin, ResizeMargin, ResizeMargin);
    }
}

} // namespace ui::widgets
