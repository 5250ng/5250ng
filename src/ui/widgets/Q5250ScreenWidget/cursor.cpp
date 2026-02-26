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

void Q5250ScreenWidget::setCursorBlinkRate(int msec) {
    m_cursorBlinkRate = msec;
    if (m_blinkTimer) {
        if (msec <= 0) {
            // Stop blinking: keep cursor always visible
            m_blinkTimer->stop();
            m_cursorBlinkState = true;
            updateCursorWidget();
            update();
        } else {
            m_blinkTimer->setInterval(msec);
            if (!m_blinkTimer->isActive()) {
                m_blinkTimer->start(msec);
            }
        }
    }
}

void Q5250ScreenWidget::setCursorEnabled(bool enabled) {
    m_cursorEnabled = enabled;
    if (m_cursorWidget) {
        m_cursorWidget->setVisible(false);
    }
    update();
}

} // namespace ui::widgets
