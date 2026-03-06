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
        }
        emit terminalStateChanged();
        update();
    }
}

void Q5250ScreenWidget::setScreenSize(int rows, int cols) {
    LOG_DEBUG(QString("[Q5250Widget] setScreenSize: %1x%2").arg(rows).arg(cols));
    m_screenBuffer->resize(rows, cols);
    calculateCellSize();
    updateGeometry();
    updateCursorWidget();
    update();
    emit screenSizeChanged(rows, cols);
}

void Q5250ScreenWidget::setGridMode(ui::themes::TerminalTheme::GridMode mode) {
    if (m_gridMode != mode) {
        m_gridMode = mode;
        calculateCellSize();
        updateGeometry();
        updateCursorWidget();
        update();
    }
}

void Q5250ScreenWidget::setFont(const QFont &font) {
    m_baseFont = font;
    m_font = font;
    calculateCellSize();
    updateGeometry();
    updateCursorWidget();
    update();
}

void Q5250ScreenWidget::applyTerminalTheme(const ui::themes::TerminalTheme &theme) {
    // Apply color scheme
    m_colorScheme = theme.buildColorScheme();

    // Apply background color
    m_bgColor = theme.backgroundColor;

    // Apply background image
    if (theme.backgroundMode == ui::themes::TerminalTheme::Image
        && !theme.backgroundImagePath.isEmpty()) {
        m_bgImage = QPixmap(theme.backgroundImagePath);
        m_hasBgImage = !m_bgImage.isNull();
    } else if (!theme.backgroundImageData.isEmpty()) {
        m_bgImage.loadFromData(theme.backgroundImageData);
        m_hasBgImage = !m_bgImage.isNull();
    } else {
        m_bgImage = QPixmap();
        m_hasBgImage = false;
    }
    m_bgImageLayout = theme.backgroundImageLayout;
    m_bgImageOpacity = theme.backgroundImageOpacity;

    // Apply foreground (use green as default foreground)
    m_fgColor = theme.colorGreen;

    // Apply font
    QFont newFont(theme.fontFamily, theme.fontSize);
    m_baseFont = newFont;
    m_font = newFont;

    // Apply cursor
    if (m_cursorWidget) {
        m_cursorWidget->setColor(theme.cursorColor);
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

    // Apply selection & indicator colors
    m_selectionBgColor = theme.selectionBackground;
    m_selectionFgColor = theme.selectionForeground;
    m_fieldIndicatorColor = theme.fieldIndicatorColor;

    // Apply column separator settings
    m_colSepColor = theme.columnSeparatorColor;
    m_colSepStyle = theme.columnSeparatorStyle;

    // Apply grid mode
    m_gridMode = theme.gridMode;

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
    uint8_t colorCode = cell.attributes.color;
    if (colorCode < m_colorScheme.size() && m_colorScheme.size() > 0) {
        fgColor = m_colorScheme[colorCode];
    } else {
        fgColor = m_fgColor;
    }

    // Reverse video
    if (cell.attributes.reverse) {
        qSwap(bgColor, fgColor);
    }

    // Fill background
    painter.fillRect(cellRect, bgColor);

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

    // Selection overlay
    if (hasSelection() && isCellSelected(row, col)) {
        painter.fillRect(cellRect, m_selectionBgColor);
    }

    // Non-display fields: draw background only, no text (e.g. password fields)
    if (cell.attributes.nonDisplay) {
        return;
    }

    // Null bytes (0x00) render as a space so underline/colSep still draw
    QChar ch = (cell.character == 0x00) ? QChar(' ') : core::EBCDIC::ebcdicToChar(cell.character);

    painter.setPen(fgColor);

    // Underline is drawn as a contiguous line in renderScreen(), not per-cell

    // Skip text when blinking is active and the blink state is "off"
    if (cell.attributes.blink && !m_blinkTextState) {
        return;
    }

    // Draw text
    painter.drawText(cellRect, Qt::AlignCenter, ch);

    // Column separator: vertical line on left edge of cell
    if (cell.attributes.colSep) {
        QPen colSepPen;
        switch (m_colSepStyle) {
        case ui::themes::TerminalTheme::Dotted:
            colSepPen = QPen(m_colSepColor, 1, Qt::DotLine);
            break;
        case ui::themes::TerminalTheme::Dimmed: {
            QColor dimmed = m_colSepColor;
            dimmed.setAlpha(dimmed.alpha() / 2);
            colSepPen = QPen(dimmed, 1, Qt::SolidLine);
            break;
        }
        case ui::themes::TerminalTheme::Solid:
        default:
            colSepPen = QPen(m_colSepColor, 1, Qt::SolidLine);
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

    // In Wide mode the grid fills the widget — no offset needed
    if (m_gridMode == ui::themes::TerminalTheme::Wide) {
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

void Q5250ScreenWidget::renderBackgroundImage(QPainter &painter) {
    if (m_bgImage.isNull()) return;

    painter.save();
    painter.setOpacity(m_bgImageOpacity);

    switch (m_bgImageLayout) {
    case ui::themes::TerminalTheme::Stretch:
        painter.drawPixmap(rect(), m_bgImage);
        break;
    case ui::themes::TerminalTheme::Fit: {
        QPixmap scaled = m_bgImage.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = (width() - scaled.width()) / 2;
        int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
        break;
    }
    case ui::themes::TerminalTheme::Center: {
        int x = (width() - m_bgImage.width()) / 2;
        int y = (height() - m_bgImage.height()) / 2;
        painter.drawPixmap(x, y, m_bgImage);
        break;
    }
    case ui::themes::TerminalTheme::Tile: {
        for (int y = 0; y < height(); y += m_bgImage.height()) {
            for (int x = 0; x < width(); x += m_bgImage.width()) {
                painter.drawPixmap(x, y, m_bgImage);
            }
        }
        break;
    }
    }

    painter.restore();
}

// CRT effect rendering moved to QCRTOverlayWidget (applied at tab level)

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

void Q5250ScreenWidget::setShowFieldProtection(bool enabled) {
    if (m_showFieldProtection == enabled) {
        return;
    }
    m_showFieldProtection = enabled;
    update();
}

void Q5250ScreenWidget::toggleFieldProtection() {
    m_showFieldProtection = !m_showFieldProtection;
    update();
}

void Q5250ScreenWidget::setShowInputFields(bool enabled) {
    if (m_showInputFields == enabled) {
        return;
    }
    m_showInputFields = enabled;
    update();
}

void Q5250ScreenWidget::toggleInputFields() {
    m_showInputFields = !m_showInputFields;
    update();
}

} // namespace ui::widgets
