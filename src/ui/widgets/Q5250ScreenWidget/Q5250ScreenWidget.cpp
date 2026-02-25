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

Q5250ScreenWidget::Q5250ScreenWidget(QWidget *parent)
    : QWidget(parent), m_screenBuffer(new ScreenBuffer(24, 80, this)),
      m_baseFont("Courier", 12), m_font("Courier", 12), m_cellSize(8, 16),
      m_bgColor(Qt::black), m_fgColor(Qt::green), m_cursorBlinkRate(250),
      m_cursorBlinkState(true), m_extendedMode(false), m_selecting(false),
      m_selectionStart(-1, -1), m_selectionEnd(-1, -1) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Initialize color scheme (standard 5250 colors)
    m_colorScheme.resize(16);
    m_colorScheme[0] = QColor(0, 0, 0);        // Black
    m_colorScheme[1] = QColor(0, 0, 128);      // Blue
    m_colorScheme[2] = QColor(0, 255, 0);      // Green (bright green #00ff00)
    m_colorScheme[3] = QColor(0, 128, 128);    // Cyan
    m_colorScheme[4] = QColor(128, 0, 0);      // Red
    m_colorScheme[5] = QColor(128, 0, 128);    // Magenta
    m_colorScheme[6] = QColor(128, 64, 0);     // Brown
    m_colorScheme[7] = QColor(192, 192, 192);  // Light gray
    m_colorScheme[8] = QColor(128, 128, 128);  // Dark gray
    m_colorScheme[9] = QColor(0, 0, 255);      // Light blue
    m_colorScheme[10] = QColor(0, 255, 0);     // Light green
    m_colorScheme[11] = QColor(0, 255, 255);   // Light cyan
    m_colorScheme[12] = QColor(255, 0, 0);     // Light red
    m_colorScheme[13] = QColor(255, 0, 255);   // Light magenta
    m_colorScheme[14] = QColor(255, 255, 0);   // Yellow
    m_colorScheme[15] = QColor(255, 255, 255); // White

    // Setup blink timer
    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, this, &Q5250ScreenWidget::onBlinkTimer);
    m_blinkTimer->start(m_cursorBlinkRate);

    // Connect screen buffer signals
    connect(m_screenBuffer, &ScreenBuffer::screenChanged, this, &Q5250ScreenWidget::onScreenChanged);
    connect(m_screenBuffer, &ScreenBuffer::cursorMoved, this, &Q5250ScreenWidget::onCursorMoved);

    // Setup integrated input handling
    m_encoder = new core::KeyboardEncoder(this);
    if (m_screenBuffer) {
        m_cursorPos = m_screenBuffer->cursorPosition();
    }

    calculateCellSize();
    m_showCursorRules = false;
    // Cursor widget overlay
    m_cursorWidget = new Q5250Cursor(this);
    m_cursorWidget->setVisible(false);
    m_cursorWidget->raise();
    m_cursorWidget->setColor(m_fgColor);
}

Q5250ScreenWidget::~Q5250ScreenWidget() {}

void Q5250ScreenWidget::setScreenSize(int rows, int cols) {
    m_screenBuffer->resize(rows, cols);
    calculateCellSize();
    updateGeometry();
    updateCursorWidget();
    update();
    emit screenSizeChanged(rows, cols);
}

void Q5250ScreenWidget::setFont(const QFont &font) {
    m_baseFont = font;
    m_font = font;
    calculateCellSize();
    updateGeometry();
    updateCursorWidget();
    update();
}

void Q5250ScreenWidget::setColorScheme(const QVector<QColor> &colors) {
    if (colors.size() >= 16) {
        m_colorScheme = colors;
        update();
    }
}

void Q5250ScreenWidget::setBackgroundColor(const QColor &color) {
    m_bgColor = color;
    update();
}

void Q5250ScreenWidget::setForegroundColor(const QColor &color) {
    m_fgColor = color;
    if (m_cursorWidget) {
        m_cursorWidget->setColor(m_fgColor);
    }
    update();
}

void Q5250ScreenWidget::updateScreen() { update(); }

void Q5250ScreenWidget::renderScreen(QPainter &painter) {
    painter.setFont(m_font);

    // Calculate offset to center the screen
    QPoint offset = screenOffset();
    painter.translate(offset);

    int rows = m_screenBuffer->rows();
    int cols = m_screenBuffer->cols();

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const ScreenCell &cell = m_screenBuffer->cell(row, col);
            renderCell(painter, row, col, cell);
        }
    }

    // Cursor rendering handled by overlay widget when enabled

    // Render selection border (outer rectangle only)
    if (hasSelection()) {
        renderSelectionBorder(painter);
    }

    // Cursor rules overlay (dashed crosshair aligned with cursor cell)
    if (m_showCursorRules) {
        renderCursorRules(painter);
    }

    // Do not emit cursor notifications from paint path to avoid repaint loops.
}

void Q5250ScreenWidget::renderCell(QPainter &painter, int row, int col, const ScreenCell &cell) {
    QRect cellRect = this->cellRect(row, col);

    // Get colors based on attributes
    QColor bgColor = m_bgColor;
    QColor fgColor = m_fgColor;

    // Apply color from cell attributes
    // Default CellAttributes has color=2 (green), which should be visible
    uint8_t colorCode = cell.attributes.color;
    if (colorCode < m_colorScheme.size() && m_colorScheme.size() > 0) {
        fgColor = m_colorScheme[colorCode];
        // Safety check: if color is black (0) on black background, use default
        // foreground This prevents invisible text
        if (colorCode == 0 && bgColor == Qt::black) {
            fgColor = m_fgColor; // Use default green instead of black
        }
    } else {
        // Fallback: if color code is out of range or color scheme is empty,
        // use default foreground color (green) to ensure visibility
        fgColor = m_fgColor;
    }

    // Reverse video
    if (cell.attributes.reverse) {
        qSwap(bgColor, fgColor);
    }

    // Fill background
    painter.fillRect(cellRect, bgColor);

    // If selected, overlay semi-transparent yellow (25% opacity)
    if (hasSelection() && isCellSelected(row, col)) {
        QColor selOverlay(255, 255, 0, static_cast<int>(255 * 0.25)); // 25% alpha
        painter.fillRect(cellRect, selOverlay);
    }

    // Draw character
    QChar ch = core::EBCDIC::ebcdicToChar(cell.character);

    painter.setPen(fgColor);

    // Handle underline
    QFont font = m_font;
    if (cell.attributes.underline) {
        font.setUnderline(true);
        painter.setFont(font);
    }

    // Skip text when blinking is active and the blink state is "off"
    if (cell.attributes.blink && !m_blinkTextState) {
        painter.setFont(m_font);
        return;
    }

    // Draw text
    painter.drawText(cellRect, Qt::AlignCenter, ch);

    // Reset font
    painter.setFont(m_font);
}

/**
 * Resolve a QColor for the given 5250 color code using the current scheme.
 * Falls back to the default foreground color when out of range.
 */
QColor Q5250ScreenWidget::getColorForCode(uint8_t colorCode) const {
    if (colorCode < m_colorScheme.size()) {
        return m_colorScheme[colorCode];
    }
    return m_fgColor;
}

/**
 * Calculate the translation offset to center the screen within the widget.
 * Offsets are clamped to non-negative values.
 */
QPoint Q5250ScreenWidget::screenOffset() const {
    if (!m_screenBuffer) {
        return QPoint(0, 0);
    }

    // Calculate total screen size
    int screenWidth = m_cellSize.width() * m_screenBuffer->cols();
    int screenHeight = m_cellSize.height() * m_screenBuffer->rows();

    // Calculate widget size
    QSize widgetSize = size();
    int widgetWidth = widgetSize.width();
    int widgetHeight = widgetSize.height();

    // Calculate offset to center the screen
    int offsetX = (widgetWidth - screenWidth) / 2;
    int offsetY = (widgetHeight - screenHeight) / 2;

    // Ensure non-negative offsets
    if (offsetX < 0) {
        offsetX = 0;
    }
    if (offsetY < 0) {
        offsetY = 0;
    }

    return QPoint(offsetX, offsetY);
}

/**
 * Convert a widget-space position to a screen cell coordinate.
 * Returns (-1,-1) if the position falls outside the screen area.
 */
QPoint Q5250ScreenWidget::screenToCell(const QPoint &screenPos) const {
    if (!m_screenBuffer) {
        return QPoint(-1, -1);
    }

    // Get screen offset
    QPoint offset = screenOffset();

    // Adjust screen position by offset
    QPoint adjustedPos = screenPos - offset;

    // Calculate cell coordinates
    int col = adjustedPos.x() / m_cellSize.width();
    int row = adjustedPos.y() / m_cellSize.height();

    // Validate coordinates
    if (row >= 0 && row < m_screenBuffer->rows() && col >= 0 &&
        col < m_screenBuffer->cols()) {
        return QPoint(col,
                      row); // Return as (col, row) to match selection coordinates
    }

    return QPoint(-1, -1);
}

void Q5250ScreenWidget::renderCursorRules(QPainter &painter) {
    if (!m_screenBuffer) {
        return;
    }
    const QPoint cursorPos = m_screenBuffer->cursorPosition(); // (row, col) but our functions expect (row,col)
    const int row = cursorPos.y();
    const int col = cursorPos.x();
    if (row < 0 || col < 0 || row >= m_screenBuffer->rows() || col >= m_screenBuffer->cols()) {
        return;
    }

    // Screen dimensions in painter's translated space
    const int screenWidth = m_cellSize.width() * m_screenBuffer->cols();
    const int screenHeight = m_cellSize.height() * m_screenBuffer->rows();

    const QRect cell = cellRect(row, col);
    // Vertical line on the left edge of the cursor cell, spanning full screen height
    const int vx = cell.left();
    // Horizontal line on the bottom edge of the cursor cell, spanning full screen width
    const int hy = cell.bottom();

    QPen pen(m_colorScheme[2]);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.save();
    painter.setPen(pen);
    // Top to bottom
    painter.drawLine(vx, 0, vx, screenHeight);
    // Right to left (draw full width; direction doesn't matter visually)
    painter.drawLine(screenWidth, hy, 0, hy);
    painter.restore();
}

void Q5250ScreenWidget::setShowCursorRules(bool enabled) {
    if (m_showCursorRules == enabled) {
        return;
    }
    m_showCursorRules = enabled;
    update();
}

void Q5250ScreenWidget::toggleCursorRules() {
    m_showCursorRules = !m_showCursorRules;
    update();
}

} // namespace ui::widgets
