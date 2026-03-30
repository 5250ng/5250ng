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

// Fixed cell aspect ratio
// This gives identical cell proportions on all platforms regardless of
// font metric differences between Windows and Linux rasterizers.
static constexpr double kCellAspectRatio = 1.0 / 1.9; // width / height

QSize Q5250ScreenWidget::sizeHint() const {
    if (m_screenBuffer) {
        QFontMetrics fm(m_font);
        int cellWidth = fm.horizontalAdvance('M');
        int cellHeight = fm.height();
        return QSize(cellWidth * m_screenBuffer->cols(),
                     cellHeight * m_screenBuffer->rows());
    }
    return QSize(640, 480);
}

QSize Q5250ScreenWidget::minimumSizeHint() const {
    return QSize(100, 100);
}

/**
 * Compute character cell size and font scaling to fit the widget dimensions.
 *
 * In Packed mode, the font aspect ratio is preserved and the grid is centered
 * within the widget (some horizontal or vertical space is unused).
 *
 * In Wide mode, cells are stretched to fill the entire widget width and height.
 * The font is scaled to fit the cell height; characters are drawn centered
 * inside their (potentially wider) cells.
 */
void Q5250ScreenWidget::calculateCellSize() {
    if (!m_screenBuffer) {
        return;
    }

    QSize widgetSize = size();
    if (widgetSize.width() <= 0 || widgetSize.height() <= 0) {
        QFontMetrics fm(m_baseFont);
        m_cellSize = QSize(fm.horizontalAdvance('M'), fm.height());
        m_font = m_baseFont;
        return;
    }

    int cols = m_screenBuffer->cols();
    int rows = m_screenBuffer->rows();

    if (cols <= 0 || rows <= 0) {
        QFontMetrics fm(m_baseFont);
        m_cellSize = QSize(fm.horizontalAdvance('M'), fm.height());
        m_font = m_baseFont;
        return;
    }

    if (m_gridMode == ui::themes::TerminalTheme::Wide) {
        // Wide mode: cells fill the entire widget
        m_cellWidthF = static_cast<qreal>(widgetSize.width()) / cols;
        m_cellHeightF = static_cast<qreal>(widgetSize.height()) / rows;
        if (m_cellWidthF < 1.0) m_cellWidthF = 1.0;
        if (m_cellHeightF < 1.0) m_cellHeightF = 1.0;
        m_cellSize = QSize(qRound(m_cellWidthF), qRound(m_cellHeightF));

        // Scale font to match cell height
        QFontMetrics fm(m_baseFont);
        int baseFontHeight = fm.height();
        if (baseFontHeight > 0 && baseFontHeight != m_cellSize.height()) {
            double scaleFactor = m_cellHeightF / baseFontHeight;
            int newPixelSize = qMax(1, qRound(m_baseFont.pixelSize() * scaleFactor));
            m_font = m_baseFont;
            m_font.setPixelSize(newPixelSize);
        } else {
            m_font = m_baseFont;
        }
    } else {
        // Packed mode: preserve font aspect ratio
        double idealCellWidth = static_cast<double>(widgetSize.width()) / cols;
        double idealCellHeight = static_cast<double>(widgetSize.height()) / rows;

        double fontAR = kCellAspectRatio;

        double cellAspectRatio = idealCellWidth / idealCellHeight;

        double finalCellWidth, finalCellHeight;
        if (cellAspectRatio > fontAR) {
            finalCellHeight = idealCellHeight;
            finalCellWidth = finalCellHeight * fontAR;
        } else {
            finalCellWidth = idealCellWidth;
            finalCellHeight = finalCellWidth / fontAR;
        }

        m_cellWidthF = finalCellWidth;
        m_cellHeightF = finalCellHeight;
        m_cellSize = QSize(qRound(finalCellWidth), qRound(finalCellHeight));

        // Scale font using fm.height() which matches QPainter::drawText layout
        QFontMetrics fm(m_baseFont);
        int baseFontHeight = fm.height();
        int targetFontHeight = m_cellSize.height();

        if (baseFontHeight != targetFontHeight && baseFontHeight > 0) {
            double scaleFactor = static_cast<double>(targetFontHeight) / baseFontHeight;
            int newPixelSize = qMax(1, qRound(m_baseFont.pixelSize() * scaleFactor));
            m_font = m_baseFont;
            m_font.setPixelSize(newPixelSize);
        } else {
            m_font = m_baseFont;
        }
    }
}

void Q5250ScreenWidget::overrideCellWidth(qreal w) {
    m_cellWidthF = w;
    m_cellSize.setWidth(qRound(w));
    updateCursorWidget();
    update();
}

void Q5250ScreenWidget::overrideCellHeight(qreal h) {
    m_cellHeightF = h;
    m_cellSize.setHeight(qRound(h));
    updateCursorWidget();
    update();
}

} // namespace ui::widgets
