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

QSize Q5250ScreenWidget::sizeHint() const {
    // Return a reasonable default size, but allow the widget to be resized
    // The actual rendering will scale to fit
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
    // Allow the widget to be resized to any size
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

    // Get available widget size
    QSize widgetSize = size();
    if (widgetSize.width() <= 0 || widgetSize.height() <= 0) {
        // Widget not yet sized, use base font metrics
        QFontMetrics fm(m_baseFont);
        m_cellSize = QSize(fm.horizontalAdvance('M'), fm.height());
        m_font = m_baseFont;
        return;
    }

    int cols = m_screenBuffer->cols();
    int rows = m_screenBuffer->rows();

    if (cols <= 0 || rows <= 0) {
        // Invalid screen size, use base font metrics
        QFontMetrics fm(m_baseFont);
        m_cellSize = QSize(fm.horizontalAdvance('M'), fm.height());
        m_font = m_baseFont;
        return;
    }

    if (m_gridMode == ui::themes::TerminalTheme::Wide) {
        // Wide mode: cells fill the entire widget
        int cellWidth = widgetSize.width() / cols;
        int cellHeight = widgetSize.height() / rows;
        if (cellWidth < 1) cellWidth = 1;
        if (cellHeight < 1) cellHeight = 1;
        m_cellSize = QSize(cellWidth, cellHeight);

        // Scale font to match cell height
        QFontMetrics fm(m_baseFont);
        int baseFontHeight = fm.height();
        if (baseFontHeight > 0 && baseFontHeight != cellHeight) {
            double scaleFactor = static_cast<double>(cellHeight) / baseFontHeight;
            double newFontSize = m_baseFont.pointSizeF() * scaleFactor;
            if (newFontSize < 1.0) newFontSize = 1.0;
            m_font = m_baseFont;
            m_font.setPointSizeF(newFontSize);
        } else {
            m_font = m_baseFont;
        }
    } else {
        // Packed mode: preserve font aspect ratio
        double idealCellWidth = static_cast<double>(widgetSize.width()) / cols;
        double idealCellHeight = static_cast<double>(widgetSize.height()) / rows;

        QFontMetrics fm(m_baseFont);
        double fontAspectRatio =
            static_cast<double>(fm.horizontalAdvance('M')) / fm.height();

        double cellAspectRatio = idealCellWidth / idealCellHeight;

        double finalCellWidth, finalCellHeight;
        if (cellAspectRatio > fontAspectRatio) {
            finalCellHeight = idealCellHeight;
            finalCellWidth = finalCellHeight * fontAspectRatio;
        } else {
            finalCellWidth = idealCellWidth;
            finalCellHeight = finalCellWidth / fontAspectRatio;
        }

        m_cellSize = QSize(static_cast<int>(finalCellWidth + 0.5),
                           static_cast<int>(finalCellHeight + 0.5));

        int baseFontHeight = fm.height();
        int targetFontHeight = m_cellSize.height();

        if (baseFontHeight != targetFontHeight && baseFontHeight > 0) {
            double scaleFactor = static_cast<double>(targetFontHeight) / baseFontHeight;
            double newFontSize = m_baseFont.pointSizeF() * scaleFactor;
            if (newFontSize < 1.0) newFontSize = 1.0;
            m_font = m_baseFont;
            m_font.setPointSizeF(newFontSize);
            // Keep the calculated m_cellSize — it is guaranteed to fit inside
            // the widget so screenOffset() can center the grid correctly.
            // Characters are drawn with Qt::AlignCenter inside each cell.
        } else {
            m_font = m_baseFont;
        }
    }
}

} // namespace ui::widgets
