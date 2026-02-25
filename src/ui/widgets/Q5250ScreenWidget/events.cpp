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

void Q5250ScreenWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Fill background
    painter.fillRect(rect(), m_bgColor);

    // Render screen
    renderScreen(painter);

}

void Q5250ScreenWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // Recalculate cell size to fit the new window size
    calculateCellSize();
    updateCursorWidget();
    update(); // Trigger repaint with new cell sizes
}

void Q5250ScreenWidget::keyPressEvent(QKeyEvent *event) {
    // Handle Ctrl+C for copying selection
    if (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_C) {
        if (hasSelection()) {
            copySelection();
            event->accept();
            return;
        }
    }

    // Handle Escape to clear selection
    if (event->key() == Qt::Key_Escape && hasSelection()) {
        clearSelection();
        event->accept();
        return;
    }

    // Handle local-only keys (block-mode: these never go to the host)
    if (m_screenBuffer) {
        switch (event->key()) {
        case Qt::Key_Left:
            moveCursorLeft();
            event->accept();
            return;
        case Qt::Key_Right:
            moveCursorRight();
            event->accept();
            return;
        case Qt::Key_Up:
            moveCursorUp();
            event->accept();
            return;
        case Qt::Key_Down:
            moveCursorDown();
            event->accept();
            return;
        case Qt::Key_Tab:
            if (event->modifiers() & Qt::ShiftModifier) {
                moveToPreviousField();
            } else {
                moveToNextField();
            }
            event->accept();
            return;
        case Qt::Key_Backtab:
            moveToPreviousField();
            event->accept();
            return;
        case Qt::Key_Home:
            moveToFieldStart();
            event->accept();
            return;
        case Qt::Key_End:
            moveToFieldEnd();
            event->accept();
            return;
        case Qt::Key_Backspace:
            handleBackspace();
            event->accept();
            return;
        case Qt::Key_Delete:
            handleDelete();
            event->accept();
            return;
        case Qt::Key_Insert:
            // Toggle insert mode (not yet tracked, just ignore)
            event->accept();
            return;
        default:
            break;
        }
    }

    // Pass other keys to input handler (AID keys and characters)
    processKeyEvent(event);
    QWidget::keyPressEvent(event);
}

void Q5250ScreenWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPoint cellPos = screenToCell(event->pos());
        if (m_screenBuffer && cellPos.x() >= 0 && cellPos.y() >= 0 &&
            cellPos.y() < m_screenBuffer->rows() && cellPos.x() < m_screenBuffer->cols()) {
            if (m_selectionEnabled) {
                // Prepare for click-or-drag: start selection at pressed cell
                m_selecting = true;
                m_selectionStart = cellPos;
                m_selectionEnd = cellPos;
                update();
                event->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void Q5250ScreenWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_selectionEnabled && m_selecting && (event->buttons() & Qt::LeftButton)) {
        QPoint cellPos = screenToCell(event->pos());
        if (cellPos.x() >= 0 && cellPos.y() >= 0) {
            m_selectionEnd = cellPos;
            update(); // Repaint to show updated selection
        }
    }
    QWidget::mouseMoveEvent(event);
}

void Q5250ScreenWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_selectionEnabled && m_screenBuffer) {
            // If no drag occurred (same cell), treat as simple click: move cursor
            if (m_selectionStart == m_selectionEnd &&
                m_selectionStart.x() >= 0 && m_selectionStart.y() >= 0) {
                m_screenBuffer->setCursorPosition(m_selectionStart.y(), m_selectionStart.x());
                // Clear selection on simple click
                m_selectionStart = QPoint(-1, -1);
                m_selectionEnd = QPoint(-1, -1);
            }
        }
        m_selecting = false;
        update(); // Final repaint (either cursor moved or selection finalized)
    }
    QWidget::mouseReleaseEvent(event);
}

void Q5250ScreenWidget::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    // Widget now has focus and can receive keyboard input
}

void Q5250ScreenWidget::focusOutEvent(QFocusEvent *event) {
    QWidget::focusOutEvent(event);
    // Widget lost focus
}

// Integrated input handling (moved from core::InputHandler)
void Q5250ScreenWidget::processKeyEvent(QKeyEvent *event) {
    if (!m_screenBuffer || !m_encoder) {
        return;
    }

    bool shiftPressed = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool ctrlPressed = (event->modifiers() & Qt::ControlModifier) != 0;
    bool altPressed = (event->modifiers() & Qt::AltModifier) != 0;

    QByteArray encoded =
        m_encoder->encodeKeyEvent(event, shiftPressed, ctrlPressed, altPressed);

    if (!encoded.isEmpty()) {
        processEncodedInput(encoded);
    }
}

void Q5250ScreenWidget::processEncodedInput(const QByteArray &data) {
    if (data.isEmpty() || !m_screenBuffer) {
        return;
    }

    uint8_t firstByte = static_cast<uint8_t>(data[0]);

    // Check if it's an AID (Attention ID) key that should be sent to host
    bool isAID = false;
    switch (firstByte) {
    case 0x7D: // Enter
    case 0x6D: // Clear
    case 0x6C: // Attn
    case 0x6F: // SysReq
    case 0xF4: // RollDown (PageDown)
    case 0xF5: // RollUp (PageUp)
        isAID = true;
        break;
    default:
        // PF1-PF9: 0xF1-0xF9
        if (firstByte >= 0xF1 && firstByte <= 0xF9) isAID = true;
        // PF10-PF12: 0x7A-0x7C
        else if (firstByte >= 0x7A && firstByte <= 0x7C) isAID = true;
        // PF13-PF21: 0xC1-0xC9
        else if (firstByte >= 0xC1 && firstByte <= 0xC9) isAID = true;
        // PF22-PF24: 0x4A-0x4C
        else if (firstByte >= 0x4A && firstByte <= 0x4C) isAID = true;
        break;
    }

    if (isAID) {
        // AID key: build field response and send to host
        emit inputReady(buildAIDResponse(firstByte));
    } else {
        // Regular EBCDIC character: write locally to screen buffer
        QPoint cursor = m_screenBuffer->cursorPosition();
        int row = cursor.y();
        int col = cursor.x();

        // Only write to unprotected positions
        if (!m_screenBuffer->isProtected(row, col)) {
            m_screenBuffer->writeChar(row, col, firstByte);
            m_screenBuffer->markFieldModified(row, col);

            // Advance cursor
            col++;
            if (col >= m_screenBuffer->cols()) {
                col = 0;
                row++;
                if (row >= m_screenBuffer->rows()) {
                    row = m_screenBuffer->rows() - 1;
                }
            }
            m_screenBuffer->setCursorPosition(row, col);
            update();
        }
    }
}

void Q5250ScreenWidget::moveToNextField() {
    if (!m_screenBuffer) {
        return;
    }

    m_cursorPos = m_screenBuffer->cursorPosition();
    ScreenBuffer::Field nextField = findNextField(m_cursorPos.y(), m_cursorPos.x());
    if (nextField.length > 0) {
        moveCursor(nextField.startRow, nextField.startCol);
    }
}

void Q5250ScreenWidget::moveToPreviousField() {
    if (!m_screenBuffer) {
        return;
    }

    m_cursorPos = m_screenBuffer->cursorPosition();
    ScreenBuffer::Field prevField = findPreviousField(m_cursorPos.y(), m_cursorPos.x());
    if (prevField.length > 0) {
        moveCursor(prevField.startRow, prevField.startCol);
    }
}

void Q5250ScreenWidget::moveToFieldStart() {
    if (!m_screenBuffer) {
        return;
    }
    m_cursorPos = m_screenBuffer->cursorPosition();
    ScreenBuffer::Field field = m_screenBuffer->getField(m_cursorPos.y(), m_cursorPos.x());
    if (field.length > 0) {
        moveCursor(field.startRow, field.startCol);
    }
}

void Q5250ScreenWidget::moveToFieldEnd() {
    if (!m_screenBuffer) {
        return;
    }
    m_cursorPos = m_screenBuffer->cursorPosition();
    ScreenBuffer::Field field = m_screenBuffer->getField(m_cursorPos.y(), m_cursorPos.x());
    if (field.length > 0) {
        int endCol = field.startCol + field.length - 1;
        moveCursor(field.startRow, endCol);
    }
}

void Q5250ScreenWidget::moveCursor(int row, int col) {
    if (m_screenBuffer) {
        m_screenBuffer->setCursorPosition(row, col);
        m_cursorPos = m_screenBuffer->cursorPosition();
    }
}

void Q5250ScreenWidget::moveCursorLeft() {
    if (!m_screenBuffer)
        return;
    m_cursorPos = m_screenBuffer->cursorPosition();
    int row = m_cursorPos.y();
    int col = m_cursorPos.x();
    if (col > 0) {
        moveCursor(row, col - 1);
    } else if (row > 0) {
        moveCursor(row - 1, m_screenBuffer->cols() - 1);
    }
}

void Q5250ScreenWidget::moveCursorRight() {
    if (!m_screenBuffer)
        return;
    m_cursorPos = m_screenBuffer->cursorPosition();
    int row = m_cursorPos.y();
    int col = m_cursorPos.x();
    if (col + 1 < m_screenBuffer->cols()) {
        moveCursor(row, col + 1);
    } else if (row + 1 < m_screenBuffer->rows()) {
        moveCursor(row + 1, 0);
    }
}

void Q5250ScreenWidget::moveCursorUp() {
    if (!m_screenBuffer)
        return;
    m_cursorPos = m_screenBuffer->cursorPosition();
    int row = m_cursorPos.y();
    int col = m_cursorPos.x();
    if (row > 0) {
        moveCursor(row - 1, col);
    }
}

void Q5250ScreenWidget::moveCursorDown() {
    if (!m_screenBuffer)
        return;
    m_cursorPos = m_screenBuffer->cursorPosition();
    int row = m_cursorPos.y();
    int col = m_cursorPos.x();
    if (row + 1 < m_screenBuffer->rows()) {
        moveCursor(row + 1, col);
    }
}

QByteArray Q5250ScreenWidget::buildAIDResponse(uint8_t aidByte) {
    QByteArray response;
    response.append(static_cast<char>(aidByte));

    if (!m_screenBuffer) {
        return response;
    }

    QPoint cursor = m_screenBuffer->cursorPosition();
    // Cursor position: 1-based row and col
    response.append(static_cast<char>(cursor.y() + 1));
    response.append(static_cast<char>(cursor.x() + 1));

    // Append modified fields: SBA(0x11) + row(1-based) + col(1-based) + field data
    QVector<ScreenBuffer::Field> modFields = m_screenBuffer->getModifiedFields();
    for (const auto &field : modFields) {
        response.append(static_cast<char>(0x11)); // SBA order
        response.append(static_cast<char>(field.startRow + 1));
        response.append(static_cast<char>(field.startCol + 1));
        response.append(m_screenBuffer->getFieldData(field));
    }

    return response;
}

void Q5250ScreenWidget::handleBackspace() {
    if (!m_screenBuffer) return;

    QPoint cursor = m_screenBuffer->cursorPosition();
    int row = cursor.y();
    int col = cursor.x();

    // Move cursor back one position
    if (col > 0) {
        col--;
    } else if (row > 0) {
        row--;
        col = m_screenBuffer->cols() - 1;
    } else {
        return; // At position (0,0), nothing to do
    }

    // Only delete if position is unprotected
    if (!m_screenBuffer->isProtected(row, col)) {
        m_screenBuffer->writeChar(row, col, 0x40); // EBCDIC space
        m_screenBuffer->markFieldModified(row, col);
        m_screenBuffer->setCursorPosition(row, col);
        update();
    }
}

void Q5250ScreenWidget::handleDelete() {
    if (!m_screenBuffer) return;

    QPoint cursor = m_screenBuffer->cursorPosition();
    int row = cursor.y();
    int col = cursor.x();

    // Only delete if position is unprotected
    if (!m_screenBuffer->isProtected(row, col)) {
        // Shift field content left from cursor to end of field
        ScreenBuffer::Field field = m_screenBuffer->getField(row, col);
        if (field.length > 0) {
            int fieldStart = field.startRow * m_screenBuffer->cols() + field.startCol;
            int fieldEnd = fieldStart + field.length;
            int curAddr = row * m_screenBuffer->cols() + col;

            for (int addr = curAddr; addr < fieldEnd - 1; ++addr) {
                int srcR = (addr + 1) / m_screenBuffer->cols();
                int srcC = (addr + 1) % m_screenBuffer->cols();
                int dstR = addr / m_screenBuffer->cols();
                int dstC = addr % m_screenBuffer->cols();
                uint8_t ch = m_screenBuffer->character(srcR, srcC);
                m_screenBuffer->writeChar(dstR, dstC, ch);
            }
            // Clear last position in field
            int lastAddr = fieldEnd - 1;
            int lastR = lastAddr / m_screenBuffer->cols();
            int lastC = lastAddr % m_screenBuffer->cols();
            m_screenBuffer->writeChar(lastR, lastC, 0x40); // EBCDIC space
            m_screenBuffer->markFieldModified(row, col);
            update();
        }
    }
}

// Private helpers
ScreenBuffer::Field Q5250ScreenWidget::findNextField(int startRow, int startCol) const {
    if (!m_screenBuffer) {
        return ScreenBuffer::Field();
    }
    for (int row = startRow; row < m_screenBuffer->rows(); ++row) {
        int startColSearch = (row == startRow) ? startCol + 1 : 0;
        for (int col = startColSearch; col < m_screenBuffer->cols(); ++col) {
            ScreenBuffer::Field field = m_screenBuffer->getField(row, col);
            if (field.length > 0 && !field.protected_field) {
                return field;
            }
        }
    }
    return ScreenBuffer::Field();
}

ScreenBuffer::Field Q5250ScreenWidget::findPreviousField(int startRow, int startCol) const {
    if (!m_screenBuffer) {
        return ScreenBuffer::Field();
    }
    for (int row = startRow; row >= 0; --row) {
        int startColSearch = (row == startRow) ? startCol - 1 : m_screenBuffer->cols() - 1;
        for (int col = startColSearch; col >= 0; --col) {
            ScreenBuffer::Field field = m_screenBuffer->getField(row, col);
            if (field.length > 0 && !field.protected_field) {
                return field;
            }
        }
    }
    return ScreenBuffer::Field();
}

bool Q5250ScreenWidget::isValidEditPosition(int row, int col) const {
    if (!m_screenBuffer) {
        return false;
    }
    if (row < 0 || row >= m_screenBuffer->rows() || col < 0 ||
        col >= m_screenBuffer->cols()) {
        return false;
    }
    if (m_screenBuffer->isProtected(row, col)) {
        return false;
    }
    return true;
}

} // namespace ui::widgets
