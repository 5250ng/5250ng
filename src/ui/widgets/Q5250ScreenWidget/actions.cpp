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

void Q5250ScreenWidget::onScreenChanged() { update(); }

void Q5250ScreenWidget::onCursorMoved(const QPoint &pos) {
    Q_UNUSED(pos);
    updateCursorWidget();
    update();
}

void Q5250ScreenWidget::onBlinkTimer() {
    m_cursorBlinkState = !m_cursorBlinkState;
    updateCursorWidget();
    if (m_screenBuffer->isCursorVisible()) {
        update();
    }
}

} // namespace ui::widgets
