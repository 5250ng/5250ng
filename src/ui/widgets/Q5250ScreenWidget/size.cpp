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
 * Preserves the base font aspect ratio to avoid distorted glyphs.
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

    // Calculate ideal cell size to fill the widget
    double idealCellWidth = static_cast<double>(widgetSize.width()) / cols;
    double idealCellHeight = static_cast<double>(widgetSize.height()) / rows;

    // Get font's natural aspect ratio from base font
    QFontMetrics fm(m_baseFont);
    double fontAspectRatio =
        static_cast<double>(fm.horizontalAdvance('M')) / fm.height();

    // Calculate cell aspect ratio
    double cellAspectRatio = idealCellWidth / idealCellHeight;

    // Adjust to maintain font aspect ratio
    double finalCellWidth, finalCellHeight;

    if (cellAspectRatio > fontAspectRatio) {
        // Widget is wider than needed, constrain by height
        finalCellHeight = idealCellHeight;
        finalCellWidth = finalCellHeight * fontAspectRatio;
    } else {
        // Widget is taller than needed, constrain by width
        finalCellWidth = idealCellWidth;
        finalCellHeight = finalCellWidth / fontAspectRatio;
    }

    // Update cell size (round to nearest integer)
    m_cellSize = QSize(static_cast<int>(finalCellWidth + 0.5),
                       static_cast<int>(finalCellHeight + 0.5));

    // Scale font to match cell size if needed
    int baseFontHeight = fm.height();
    int targetFontHeight = m_cellSize.height();

    if (baseFontHeight != targetFontHeight && baseFontHeight > 0) {
        // Scale font to fit the cell height
        double scaleFactor = static_cast<double>(targetFontHeight) / baseFontHeight;
        double newFontSize = m_baseFont.pointSizeF() * scaleFactor;
        if (newFontSize < 1.0) {
            newFontSize = 1.0;
        }
        m_font = m_baseFont;
        m_font.setPointSizeF(newFontSize);

        // Recalculate cell size from scaled font to ensure exact fit
        QFontMetrics scaledFm(m_font);
        m_cellSize = QSize(scaledFm.horizontalAdvance('M'), scaledFm.height());
    } else {
        // Use base font if no scaling needed
        m_font = m_baseFont;
        // Cell size is already calculated to fit the window
    }
}

} // namespace ui::widgets
