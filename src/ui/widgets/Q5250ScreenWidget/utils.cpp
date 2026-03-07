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
 * Return the top-left pixel position of the given cell.
 *
 * Uses floating-point cell dimensions to distribute sub-pixel rounding
 * evenly across the grid, preventing cumulative drift at high column counts.
 */
QPoint Q5250ScreenWidget::cellPosition(int row, int col) const {
    return QPoint(qRound(col * m_cellWidthF), qRound(row * m_cellHeightF));
}

/**
 * Compute the pixel rectangle bounding the given cell.
 *
 * Width/height are derived from the difference between adjacent cell
 * positions so that cells tile perfectly without gaps or overlaps.
 */
QRect Q5250ScreenWidget::cellRect(int row, int col) const {
    int x = qRound(col * m_cellWidthF);
    int y = qRound(row * m_cellHeightF);
    int x1 = qRound((col + 1) * m_cellWidthF);
    int y1 = qRound((row + 1) * m_cellHeightF);
    return QRect(x, y, x1 - x, y1 - y);
}

void Q5250ScreenWidget::updateCursorWidget() {
    if (!m_cursorWidget || !m_screenBuffer) {
        return;
    }
    if (!m_cursorEnabled) {
        m_cursorWidget->setVisible(false);
        return;
    }
    QPoint cursorPos = m_screenBuffer->cursorPosition(); // (row, col)
    int row = cursorPos.y();
    int col = cursorPos.x();
    if (row < 0 || col < 0 || row >= m_screenBuffer->rows() || col >= m_screenBuffer->cols()) {
        m_cursorWidget->setVisible(false);
        return;
    }
    // Compute geometry in widget coordinates
    QRect cell = cellRect(row, col);
    QPoint offset = screenOffset();
    QRect geo(cell.translated(offset));
    m_cursorWidget->setGeometry(geo);
    // Visibility controlled by blink timer
    bool visible = m_screenBuffer->isCursorVisible() && m_cursorBlinkState && m_cursorEnabled;
    m_cursorWidget->setVisible(visible);
}

} // namespace ui::widgets
