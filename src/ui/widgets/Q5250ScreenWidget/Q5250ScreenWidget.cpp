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
#include "logger/logger.h"
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
 * @brief Constructs the 5250 screen widget with default 24×80 screen and theme.
 * @param parent Optional parent widget.
 *
 * Initializes the screen buffer, 16-color scheme, blink timer, keyboard encoder,
 * cursor overlay, and terminal state (keyboard unlocked, insert mode off).
 */
Q5250ScreenWidget::Q5250ScreenWidget(QWidget *parent)
    : QWidget(parent), m_screenBuffer(new ScreenBuffer(24, 80, this)),
      m_baseFont("Courier"), m_font("Courier"), m_cellSize(8, 16),
      m_bgColor(Qt::black), m_fgColor(Qt::green), m_cursorBlinkRate(250),
      m_cursorBlinkState(true), m_extendedMode(false), m_selecting(false),
      m_selectionStart(-1, -1), m_selectionEnd(-1, -1) {
    m_baseFont.setPixelSize(12);
    m_font.setPixelSize(12);

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
    m_showFieldProtection = false;
    m_showInputFields = false;

    // Initialize terminal state
    m_keyboardState = KeyboardState::Unlocked;
    m_insertMode = false;
    m_messageWaiting = false;
    m_icRow = 0;
    m_icCol = 0;
    m_errorLineRow = -1; // Default: last row
    m_cmdKeyMask[0] = m_cmdKeyMask[1] = m_cmdKeyMask[2] = 0;

    // Cursor widget overlay
    m_cursorWidget = new Q5250Cursor(this);
    m_cursorWidget->setVisible(false);
    m_cursorWidget->raise();
    m_cursorWidget->setColor(m_fgColor);
}

Q5250ScreenWidget::~Q5250ScreenWidget() {}

/**
 * @brief Sets the 5250 keyboard state (Unlocked, Locked, ErrorLocked, SystemRequest).
 * @param state New keyboard state.
 *
 * Exits insert mode when locking. Emits terminalStateChanged() and schedules a repaint.
 */
void Q5250ScreenWidget::setKeyboardState(KeyboardState state) {
    if (m_keyboardState != state) {
        const char *names[] = {"Unlocked", "Locked", "ErrorLocked", "SystemRequest"};
        int oldIdx = static_cast<int>(m_keyboardState);
        int newIdx = static_cast<int>(state);
        LOG_DEBUG(QString("[Q5250Widget] setKeyboardState: %1 -> %2")
            .arg(oldIdx < 4 ? names[oldIdx] : "?")
            .arg(newIdx < 4 ? names[newIdx] : "?"));
        m_keyboardState = state;
        // Exiting insert mode when keyboard locks (per spec)
        if (state == KeyboardState::Locked || state == KeyboardState::ErrorLocked) {
            m_insertMode = false;
            // Hide cursor during host processing (per 5250 spec)
            if (m_screenBuffer)
                m_screenBuffer->setCursorVisible(false);
            updateCursorWidget();
        }
        emit terminalStateChanged();
        update();
    }
}

/**
 * @brief Resizes the screen buffer and updates layout.
 * @param rows Number of rows.
 * @param cols Number of columns.
 *
 * Recalculates cell size, updates geometry and cursor widget, then emits screenSizeChanged().
 */
void Q5250ScreenWidget::setScreenSize(int rows, int cols) {
    LOG_DEBUG(QString("[Q5250Widget] setScreenSize: %1x%2").arg(rows).arg(cols));
    m_screenBuffer->resize(rows, cols);
    calculateCellSize();
    updateGeometry();
    updateCursorWidget();
    update();
    emit screenSizeChanged(rows, cols);
}

/**
 * @brief Sets the grid layout mode (e.g. Packed vs Wide).
 * @param mode New grid mode.
 *
 * Recalculates cell size and updates geometry and cursor when the mode changes.
 */
void Q5250ScreenWidget::setGridMode(ui::themes::TerminalTheme::GridMode mode) {
    if (m_gridMode != mode) {
        m_gridMode = mode;
        calculateCellSize();
        updateGeometry();
        updateCursorWidget();
        update();
    }
}

/**
 * @brief Sets the base and rendering font for the terminal.
 * @param font New font.
 *
 * Recalculates cell size, updates geometry and cursor, and repaints.
 */
void Q5250ScreenWidget::setFont(const QFont &font) {
    m_baseFont = font;
    m_font = font;
    calculateCellSize();
    updateGeometry();
    updateCursorWidget();
    update();
}

/**
 * @brief Applies a terminal theme (colors, font, cursor, grid, overlays).
 * @param theme Theme to apply.
 *
 * Updates color scheme, background/foreground, font, cursor widget, blink rate,
 * selection/indicator colors, column separator and cell grid, then recalculates layout.
 */
void Q5250ScreenWidget::applyTerminalTheme(const ui::themes::TerminalTheme &theme) {
    // Apply color scheme
    m_colorScheme = theme.buildColorScheme();

    // Apply background color (with brightness/saturation adjustment)
    m_bgColor = theme.adjustColor(theme.backgroundColor);

    // Background image is now rendered at the tab container level (QBackgroundImageWidget).
    // When a background image is active, screen background is painted with reduced opacity
    // so the image shows through.
    m_bgOpacity = (theme.backgroundMode == ui::themes::TerminalTheme::Image)
                      ? theme.screenBackgroundOpacity : 1.0;
    bool transparent = (m_bgOpacity < 1.0);
    setAttribute(Qt::WA_TranslucentBackground, transparent);
    setAttribute(Qt::WA_OpaquePaintEvent, !transparent);
    setAutoFillBackground(false);

    // Apply foreground (use green as default foreground, with brightness/saturation adjustment)
    m_fgColor = theme.adjustColor(theme.colorGreen);

    // Apply font
    QFont newFont(theme.fontFamily);
    newFont.setPixelSize(theme.fontSize);
    m_baseFont = newFont;
    m_font = newFont;

    // Apply cursor
    if (m_cursorWidget) {
        m_cursorWidget->setColor(theme.adjustColor(theme.cursorColor));
        m_cursorWidget->setCursorShape(theme.cursorShape);
    }
    m_cursorBlinkRate = theme.cursorBlinkRateMs;
    if (m_blinkTimer) {
        if (m_cursorBlinkRate > 0) {
            m_blinkTimer->setInterval(m_cursorBlinkRate);
            if (!m_blinkTimer->isActive()) {
                m_blinkTimer->start(m_cursorBlinkRate);
            }
        } else {
            m_blinkTimer->stop();
            m_cursorBlinkState = true;
        }
    }

    // Apply selection & indicator colors (with brightness/saturation adjustment)
    m_selectionBgColor = theme.adjustColor(theme.selectionBackground);
    m_selectionFgColor = theme.adjustColor(theme.selectionForeground);
    m_selectionBorderColor = theme.adjustColor(theme.selectionBorder);
    m_fieldIndicatorColor = theme.adjustColor(theme.fieldIndicatorColor);

    // Apply column separator settings
    m_colSepEnabled = theme.columnSeparatorEnabled;
    m_colSepColor = theme.adjustColor(theme.columnSeparatorColor);
    m_colSepStyle = theme.columnSeparatorStyle;

    // Apply cell grid color
    m_cellGridColor = theme.adjustColor(theme.cellGridColor);

    // Apply grid mode
    m_gridMode = theme.gridMode;

    calculateCellSize();
    updateGeometry();
    updateCursorWidget();
    update();
}

/**
 * @brief Sets the 16-color scheme used for cell attributes.
 * @param colors Vector of at least 16 colors. No change if size &lt; 16.
 */
void Q5250ScreenWidget::setColorScheme(const QVector<QColor> &colors) {
    if (colors.size() >= 16) {
        m_colorScheme = colors;
        update();
    }
}

/**
 * @brief Sets the default background color for the screen.
 * @param color New background color.
 */
void Q5250ScreenWidget::setBackgroundColor(const QColor &color) {
    m_bgColor = color;
    update();
}

/**
 * @brief Sets the default foreground color and updates the cursor widget color.
 * @param color New foreground color.
 */
void Q5250ScreenWidget::setForegroundColor(const QColor &color) {
    m_fgColor = color;
    if (m_cursorWidget) {
        m_cursorWidget->setColor(m_fgColor);
    }
    update();
}

/**
 * @brief Schedules a repaint of the widget.
 */
void Q5250ScreenWidget::updateScreen() { update(); }

void Q5250ScreenWidget::setGddmGraphicsPlane(const QImage &plane, bool visible) {
    m_gddmGraphicsPlane = plane;
    m_gddmGraphicsVisible = visible;
    update();
}

/**
 * @brief Renders the full 5250 screen: cells, underlines, grid, selection, cursor rules, hotspots.
 * @param painter Painter already set up for this widget; clipping and translation applied here.
 *
 * Draws cells (with theme/attributes), contiguous underlines, optional cell grid,
 * selection border, hotspot underlines, and cursor rules overlay when enabled.
 */
void Q5250ScreenWidget::renderScreen(QPainter &painter) {
    painter.setFont(m_font);

    // Calculate offset to center the screen
    QPoint offset = screenOffset();

    // Clip to widget bounds so grid content doesn't overflow when
    // the widget is smaller than the calculated grid (e.g., during resize)
    painter.save();
    painter.setClipRect(rect());
    painter.translate(offset);

    int rows = m_screenBuffer->rows();
    int cols = m_screenBuffer->cols();

    if (m_gddmGraphicsVisible && !m_gddmGraphicsPlane.isNull()) {
        // A 5292 presents graphics as the background to ALWGPH alphanumeric
        // fields. Map the device plane to the complete logical character grid
        // so device coordinates stay aligned with display-file positions.
        const QRectF presentationRect(0.0, 0.0, cols * m_cellWidthF,
                                      rows * m_cellHeightF);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(presentationRect, m_gddmGraphicsPlane);
    }

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const ScreenCell &cell = m_screenBuffer->cell(row, col);
            renderCell(painter, row, col, cell);
        }
    }

    // Draw contiguous underlines: scan each row for runs of underlined cells
    // and draw a single line spanning the full run (no gaps between cells).
    for (int row = 0; row < rows; ++row) {
        int col = 0;
        while (col < cols) {
            const ScreenCell &cell = m_screenBuffer->cell(row, col);
            if (cell.attributes.underline && !cell.attributes.nonDisplay) {
                int runStart = col;
                QColor ulColor = getColorForCode(cell.attributes.color);
                while (col < cols) {
                    const ScreenCell &c = m_screenBuffer->cell(row, col);
                    if (!c.attributes.underline || c.attributes.nonDisplay)
                        break;
                    ++col;
                }
                QRect startRect = cellRect(row, runStart);
                QRect endRect = cellRect(row, col - 1);
                int y = endRect.bottom();
                painter.setPen(ulColor);
                painter.drawLine(startRect.left(), y, endRect.right(), y);
            } else {
                ++col;
            }
        }
    }

    // Cell grid overlay (drawn on top of cell backgrounds, under selection/cursor)
    if (m_showCellGrid) {
        renderCellGrid(painter);
    }

    // Cursor rendering handled by overlay widget when enabled

    // Render selection border (outer rectangle only)
    if (hasSelection()) {
        renderSelectionBorder(painter);
    }

    // Hotspot underlines
    if (m_hotspotDetector.isEnabled()) {
        renderHotspots(painter);
    }

    // Cursor rules overlay (dashed crosshair aligned with cursor cell)
    if (m_showCursorRules) {
        renderCursorRules(painter);
    }

    // Do not emit cursor notifications from paint path to avoid repaint loops.
    painter.restore();
}

/**
 * @brief Renders a single screen cell (background, overlays, text, column separator).
 * @param painter Painter in widget coordinates (after screen offset).
 * @param row Cell row.
 * @param col Cell column.
 * @param cell Cell data (character, attributes).
 *
 * Handles color scheme, reverse video, field protection/input overlays, selection,
 * non-display (password) fields, match-replace overlay, blink, and column separator.
 */
void Q5250ScreenWidget::renderCell(QPainter &painter, int row, int col, const ScreenCell &cell) {
    QRect cellRect = this->cellRect(row, col);

    // Get colors based on attributes
    QColor bgColor = m_bgColor;
    QColor fgColor = m_fgColor;

    // Apply color from cell attributes
    uint8_t colorCode = cell.attributes.color;
    if (colorCode < m_colorScheme.size() && m_colorScheme.size() > 0) {
        fgColor = m_colorScheme[colorCode];
    } else {
        fgColor = m_fgColor;
    }

    // Reverse video (not applied to nonDisplay marker cells - they always use default bg)
    if (cell.attributes.reverse && !cell.attributes.nonDisplay) {
        qSwap(bgColor, fgColor);
    }

    // In 5292 graphics display mode, ordinary cell backgrounds are
    // transparent so the graphics picture remains visible behind A/N data.
    // Reverse video still supplies an explicit alphanumeric background.
    if (!m_gddmGraphicsVisible
        || (cell.attributes.reverse && !cell.attributes.nonDisplay)) {
        painter.fillRect(cellRect, bgColor);
    }

    // Field protection overlay
    if (m_showFieldProtection) {
        if (cell.attributes.protected_field) {
            painter.fillRect(cellRect, QColor(255, 0, 0, 64));   // Light red 25%
        } else {
            painter.fillRect(cellRect, QColor(0, 255, 0, 64));   // Light green 25%
        }
    }

    // Input fields overlay
    if (m_showInputFields && m_screenBuffer->isInField(row, col)) {
        painter.fillRect(cellRect, m_fieldIndicatorColor);
    }

    // Selection overlay: fill with selection background and override text color
    const bool cellSelected = hasSelection() && isCellSelected(row, col);
    if (cellSelected) {
        painter.fillRect(cellRect, m_selectionBgColor);
        // Only override the text color when the selection FG is fully opaque;
        // a transparent selection FG (alpha 0) means "keep the original color".
        if (m_selectionFgColor.alpha() > 0) {
            fgColor = m_selectionFgColor;
        }
    }

    // Non-display fields: draw background only, no text (e.g. password fields)
    if (cell.attributes.nonDisplay) {
        return;
    }

    // Null bytes (0x00) render as a space so underline/colSep still draw
    QChar ch;
    if (m_matchReplace && m_matchReplace->isEnabled() && m_matchReplace->hasOverlay(row, col)) {
        ch = m_matchReplace->overlayChar(row, col);
    } else {
        ch = (cell.character == 0x00) ? QChar(' ') : core::EBCDIC::ebcdicToChar(cell.character);
    }

    painter.setPen(fgColor);

    // Underline is drawn as a contiguous line in renderScreen(), not per-cell

    // Skip text when blinking is active and the blink state is "off"
    if (cell.attributes.blink && !m_blinkTextState) {
        return;
    }

    // Draw text
    painter.drawText(cellRect, Qt::AlignCenter, ch);

    // Column separator: vertical line on left edge of cell
    if (m_colSepEnabled && cell.attributes.colSep) {
        // DPI-aware pen width so separators remain visible on high-DPI
        qreal penW = qMax(1.0, devicePixelRatioF());
        QPen colSepPen;
        switch (m_colSepStyle) {
        case ui::themes::TerminalTheme::Dotted:
            colSepPen = QPen(m_colSepColor, penW, Qt::DotLine);
            break;
        case ui::themes::TerminalTheme::Dimmed: {
            QColor dimmed = m_colSepColor;
            dimmed.setAlpha(dimmed.alpha() / 2);
            colSepPen = QPen(dimmed, penW, Qt::SolidLine);
            break;
        }
        case ui::themes::TerminalTheme::Solid:
        default:
            colSepPen = QPen(m_colSepColor, penW, Qt::SolidLine);
            break;
        }
        painter.setPen(colSepPen);
        painter.drawLine(cellRect.left(), cellRect.top(), cellRect.left(), cellRect.bottom());
    }
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

    // In Wide mode the grid fills the widget - no offset needed
    if (m_gridMode == ui::themes::TerminalTheme::Wide) {
        return QPoint(0, 0);
    }

    // Use floating-point cell dimensions for precise centering
    int screenWidth = qRound(m_cellWidthF * m_screenBuffer->cols());

    QSize widgetSize = size();
    int offsetX = (widgetSize.width() - screenWidth) / 2;

    // Clamp to non-negative; if widget is smaller than grid, anchor left
    if (offsetX < 0) offsetX = 0;

    // Anchor grid to top so there is no gap between the last row and the hrule
    return QPoint(offsetX, 0);
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

    // Calculate cell coordinates (guard against zero cell size)
    if (m_cellSize.width() <= 0 || m_cellSize.height() <= 0)
        return QPoint(-1, -1);
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

/**
 * @brief Draws dashed crosshair lines aligned with the cursor cell (vertical and horizontal).
 * @param painter Painter in translated screen coordinates.
 *
 * No-op if there is no screen buffer or cursor is out of range.
 */
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

    QPen pen(m_colorScheme[10]);
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

/**
 * @brief Draws a grid overlay over all cell boundaries (row and column lines).
 * @param painter Painter in translated screen coordinates.
 *
 * No-op if there is no screen buffer.
 */
void Q5250ScreenWidget::renderCellGrid(QPainter &painter) {
    if (!m_screenBuffer) {
        return;
    }

    int rows = m_screenBuffer->rows();
    int cols = m_screenBuffer->cols();

    int screenWidth = qRound(m_cellWidthF * cols);
    int screenHeight = qRound(m_cellHeightF * rows);

    QPen pen(m_cellGridColor);
    pen.setWidth(1);
    painter.save();
    painter.setPen(pen);

    // Horizontal lines (row boundaries)
    for (int row = 0; row <= rows; ++row) {
        int y = qRound(row * m_cellHeightF);
        painter.drawLine(0, y, screenWidth, y);
    }

    // Vertical lines (column boundaries)
    for (int col = 0; col <= cols; ++col) {
        int x = qRound(col * m_cellWidthF);
        painter.drawLine(x, 0, x, screenHeight);
    }

    painter.restore();
}

// Background image rendering moved to QBackgroundImageWidget (applied at tab level)
// CRT effect rendering moved to QCRTOverlayWidget (applied at tab level)

/**
 * @brief Enables or disables the cursor rules (crosshair) overlay.
 * @param enabled true to show, false to hide.
 */
void Q5250ScreenWidget::setShowCursorRules(bool enabled) {
    if (m_showCursorRules == enabled) {
        return;
    }
    m_showCursorRules = enabled;
    update();
}

/**
 * @brief Toggles the cursor rules overlay on or off.
 */
void Q5250ScreenWidget::toggleCursorRules() {
    m_showCursorRules = !m_showCursorRules;
    update();
}

/**
 * @brief Enables or disables the field protection overlay (red/green tint per cell).
 * @param enabled true to show, false to hide.
 */
void Q5250ScreenWidget::setShowFieldProtection(bool enabled) {
    if (m_showFieldProtection == enabled) {
        return;
    }
    m_showFieldProtection = enabled;
    update();
}

/**
 * @brief Toggles the field protection overlay on or off.
 */
void Q5250ScreenWidget::toggleFieldProtection() {
    m_showFieldProtection = !m_showFieldProtection;
    update();
}

/**
 * @brief Enables or disables the input-fields overlay (highlights editable fields).
 * @param enabled true to show, false to hide.
 */
void Q5250ScreenWidget::setShowInputFields(bool enabled) {
    if (m_showInputFields == enabled) {
        return;
    }
    m_showInputFields = enabled;
    update();
}

/**
 * @brief Toggles the input fields overlay on or off.
 */
void Q5250ScreenWidget::toggleInputFields() {
    m_showInputFields = !m_showInputFields;
    update();
}

/**
 * @brief Enables or disables the cell grid overlay (lines between cells).
 * @param enabled true to show, false to hide.
 */
void Q5250ScreenWidget::setShowCellGrid(bool enabled) {
    if (m_showCellGrid == enabled) {
        return;
    }
    m_showCellGrid = enabled;
    update();
}

/**
 * @brief Toggles the cell grid overlay on or off.
 */
void Q5250ScreenWidget::toggleCellGrid() {
    m_showCellGrid = !m_showCellGrid;
    update();
}

// --- Hotspots ---

/**
 * @brief Enables or disables hotspot detection and underlines.
 * @param enabled true to detect and draw hotspots; false clears the list and hides underlines.
 */
void Q5250ScreenWidget::setHotspotsEnabled(bool enabled) {
    m_hotspotDetector.setEnabled(enabled);
    if (enabled) refreshHotspots();
    else m_hotspots.clear();
    update();
}

/**
 * @brief Toggles hotspot detection and display on or off.
 */
void Q5250ScreenWidget::toggleHotspots() {
    setHotspotsEnabled(!m_hotspotDetector.isEnabled());
}

/**
 * @brief Re-scans the screen buffer and updates the list of detected hotspots.
 *
 * No-op if hotspots are disabled or there is no screen buffer.
 */
void Q5250ScreenWidget::refreshHotspots() {
    if (!m_hotspotDetector.isEnabled() || !m_screenBuffer) {
        m_hotspots.clear();
        return;
    }
    int rows = m_screenBuffer->rows();
    int cols = m_screenBuffer->cols();
    QVector<QChar> text(rows * cols);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            text[r * cols + c] = core::EBCDIC::ebcdicToChar(m_screenBuffer->cell(r, c).character);
    m_hotspots = m_hotspotDetector.detect(text, rows, cols);
}

/**
 * @brief Draws dashed underlines for each detected hotspot.
 * @param painter Painter in translated screen coordinates.
 *
 * No-op if the hotspot list is empty.
 */
void Q5250ScreenWidget::renderHotspots(QPainter &painter) {
    if (m_hotspots.isEmpty()) return;

    QPen pen(QColor(0, 180, 255, 180));
    pen.setWidth(1);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);

    for (const auto &h : m_hotspots) {
        QRect start = cellRect(h.row, h.startCol);
        QRect end = cellRect(h.row, h.endCol);
        int y = end.bottom();
        painter.drawLine(start.left(), y, end.right(), y);
    }
}

// --- Screen History ---

/**
 * @brief Pushes the current screen contents and cursor position onto the history stack.
 *
 * No-op if there is no screen buffer. Used for scrollback / screen history.
 */
void Q5250ScreenWidget::pushScreenToHistory() {
    if (!m_screenBuffer) return;
    core::ScreenSnapshot snap;
    snap.timestamp = QDateTime::currentDateTime();
    snap.rows = m_screenBuffer->rows();
    snap.cols = m_screenBuffer->cols();
    QPoint cur = m_screenBuffer->cursorPosition();
    snap.cursorRow = cur.y();
    snap.cursorCol = cur.x();

    // Serialize screen text as UTF-8
    QString text;
    text.reserve(snap.rows * snap.cols);
    for (int r = 0; r < snap.rows; ++r)
        for (int c = 0; c < snap.cols; ++c)
            text.append(core::EBCDIC::ebcdicToChar(m_screenBuffer->cell(r, c).character));
    snap.cellData = text.toUtf8();
    m_screenHistory.push(snap);
}

/**
 * @brief Switches the display to a historical screen from the history stack.
 * @param index Index into the history (0 = most recent). Ignored if out of range.
 *
 * Emits historyViewChanged(index, total) and repaints.
 */
void Q5250ScreenWidget::viewHistoryScreen(int index) {
    if (index < 0 || index >= m_screenHistory.count()) return;
    m_historyIndex = index;
    emit historyViewChanged(index, m_screenHistory.count());
    update();
}

/**
 * @brief Returns the display to the live screen (exits history view).
 *
 * Sets history index to -1, emits historyViewChanged(-1, total), and repaints.
 */
void Q5250ScreenWidget::exitHistoryView() {
    m_historyIndex = -1;
    emit historyViewChanged(-1, m_screenHistory.count());
    update();
}

} // namespace ui::widgets
