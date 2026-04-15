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
 * Draw a border around the current selection rectangle, if any.
 * The border is a 2px rectangle in the theme's selection-border color
 * around the union of selected cells.
 */
void Q5250ScreenWidget::renderSelectionBorder(QPainter &painter) {
    if (!hasSelection() || !m_screenBuffer) {
        return;
    }

    // Normalize selection coordinates
    int startRow = qMin(m_selectionStart.y(), m_selectionEnd.y());
    int endRow = qMax(m_selectionStart.y(), m_selectionEnd.y());
    int startCol = qMin(m_selectionStart.x(), m_selectionEnd.x());
    int endCol = qMax(m_selectionStart.x(), m_selectionEnd.x());

    // Get the rectangle for the first and last cells
    QRect topLeftCell = cellRect(startRow, startCol);
    QRect bottomRightCell = cellRect(endRow, endCol);

    // Calculate the outer border rectangle
    QRect selectionRect =
        QRect(topLeftCell.topLeft(), bottomRightCell.bottomRight());

    // Draw themed border around the entire selection
    QPen borderPen(m_selectionBorderColor, 2);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selectionRect.adjusted(1, 1, -1, -1)); // Slightly inset
}

/**
 * Check whether a given cell is currently within the selection rectangle.
 */
bool Q5250ScreenWidget::isCellSelected(int row, int col) const {
    if (m_selectionStart.x() < 0 || m_selectionStart.y() < 0 ||
        m_selectionEnd.x() < 0 || m_selectionEnd.y() < 0) {
        return false;
    }

    // Normalize selection coordinates
    int startRow = qMin(m_selectionStart.y(), m_selectionEnd.y());
    int endRow = qMax(m_selectionStart.y(), m_selectionEnd.y());
    int startCol = qMin(m_selectionStart.x(), m_selectionEnd.x());
    int endCol = qMax(m_selectionStart.x(), m_selectionEnd.x());

    // Check if cell is within selection rectangle
    return row >= startRow && row <= endRow && col >= startCol && col <= endCol;
}

/**
 * Return true if there is a non-empty selection active.
 */
bool Q5250ScreenWidget::hasSelection() const {
    if (!m_selectionEnabled) {
        return false;
    }
    return m_selectionStart.x() >= 0 && m_selectionStart.y() >= 0 &&
           m_selectionEnd.x() >= 0 && m_selectionEnd.y() >= 0 &&
           (m_selectionStart != m_selectionEnd);
}

/**
 * Copy the current selection to the system clipboard as plain text.
 * Converts EBCDIC to QChar; nulls and EBCDIC spaces are emitted as spaces.
 */
void Q5250ScreenWidget::copySelection() {
    if (!m_selectionEnabled || !hasSelection() || !m_screenBuffer) {
        return;
    }

    // Normalize selection coordinates
    int startRow = qMin(m_selectionStart.y(), m_selectionEnd.y());
    int endRow = qMax(m_selectionStart.y(), m_selectionEnd.y());
    int startCol = qMin(m_selectionStart.x(), m_selectionEnd.x());
    int endCol = qMax(m_selectionStart.x(), m_selectionEnd.x());

    // Build text string from selection
    QString text;
    int maxRows = m_screenBuffer->rows();
    int maxCols = m_screenBuffer->cols();

    for (int row = startRow; row <= endRow && row < maxRows; ++row) {
        QString line;
        for (int col = startCol; col <= endCol && col < maxCols; ++col) {
            if (row >= 0 && col >= 0) {
                uint8_t ebcdicChar = m_screenBuffer->character(row, col);
                QChar ch = core::EBCDIC::ebcdicToChar(ebcdicChar);

                // Only replace null bytes and EBCDIC spaces with ASCII space to
                // preserve formatting Keep all other characters including non-printable
                // ones
                if (ebcdicChar == 0x00 || // Null byte
                    ebcdicChar == 0x40) { // EBCDIC space
                    ch = QChar(' ');      // Replace with space
                }

                line.append(ch);
            } else {
                // Out of bounds - add space to maintain alignment
                line.append(' ');
            }
        }
        if (row < endRow) {
            line.append('\n'); // Add newline between rows
        }
        text.append(line);
    }

    // Copy to clipboard
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
    }
}

/**
 * Clear any active selection and repaint.
 */
void Q5250ScreenWidget::clearSelection() {
    m_selectionStart = QPoint(-1, -1);
    m_selectionEnd = QPoint(-1, -1);
    m_selecting = false;
    update();
}

void Q5250ScreenWidget::selectAll() {
    if (!m_screenBuffer) return;
    m_selectionStart = QPoint(0, 0);
    m_selectionEnd = QPoint(m_screenBuffer->cols() - 1, m_screenBuffer->rows() - 1);
    m_selecting = false;
    update();
}

} // namespace ui::widgets
