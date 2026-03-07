#include "Q5250ScreenWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

namespace ui::widgets {

void Q5250ScreenWidget::onScreenChanged() {
    // Push to screen history and refresh hotspots on each screen update
    pushScreenToHistory();
    if (m_hotspotDetector.isEnabled())
        refreshHotspots();
    update();
}

void Q5250ScreenWidget::onCursorMoved(const QPoint &pos) {
    Q_UNUSED(pos);
    updateCursorWidget();
    update();
}

void Q5250ScreenWidget::onBlinkTimer() {
    m_cursorBlinkState = !m_cursorBlinkState;
    m_blinkTextState = !m_blinkTextState;
    updateCursorWidget();
    update();
}

} // namespace ui::widgets
