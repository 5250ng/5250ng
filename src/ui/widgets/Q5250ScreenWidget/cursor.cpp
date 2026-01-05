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

/**
 * Draw the visual cursor at the given row/col cell.
 * Currently rendered as an underline block using the foreground color.
 */
void Q5250ScreenWidget::renderCursor(QPainter &painter, int row, int col) {
    QRect cellRect = this->cellRect(row, col);

    // Draw cursor as a full block
    painter.fillRect(cellRect, m_fgColor);
}

void Q5250ScreenWidget::setCursorBlinkRate(int msec) {
    m_cursorBlinkRate = msec;
    if (m_blinkTimer) {
        m_blinkTimer->setInterval(msec);
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
