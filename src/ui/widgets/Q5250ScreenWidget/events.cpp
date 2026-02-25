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

    // Handle arrow keys for cursor movement
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
        default:
            break;
        }
    }

    // Pass other keys to input handler
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

    // Check if it's an AID (Attention ID) - special key or PF key
    bool isAID = (firstByte >= 0x6C && firstByte <= 0xFF) ||
                 (firstByte >= 0xF1 && firstByte <= 0xF9) ||
                 (firstByte >= 0x7A && firstByte <= 0x7D) ||
                 (firstByte >= 0xC1 && firstByte <= 0xC9) ||
                 (firstByte >= 0x4A && firstByte <= 0x4C);

    // Always send input to server. Do not locally modify the buffer here to
    // avoid double-echo; the server returns the updated screen.
    Q_UNUSED(isAID);
    emit inputReady(data);
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
