#include "Q5250ScreenWidget.h"
#include <climits>
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

bool Q5250ScreenWidget::event(QEvent *ev) {
    // Intercept Tab/Backtab before Qt's focus-chain machinery consumes them.
    // QWidget::event() normally converts these into focus-next/focus-prev and
    // never calls keyPressEvent, so we must catch them here.
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            keyPressEvent(ke);
            return true;
        }
    }
    return QWidget::event(ev);
}

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

    // Handle Escape: Error Reset (if error-locked) or clear selection
    if (event->key() == Qt::Key_Escape) {
        if (m_keyboardState == KeyboardState::ErrorLocked) {
            // Error Reset: restore error line and unlock keyboard
            if (!m_savedErrorLine.isEmpty() && m_screenBuffer) {
                int errRow = (m_errorLineRow >= 0) ? m_errorLineRow : (m_screenBuffer->rows() - 1);
                for (int c = 0; c < m_screenBuffer->cols() && c < m_savedErrorLine.size(); ++c) {
                    m_screenBuffer->cell(errRow, c) = m_savedErrorLine[c];
                }
                m_savedErrorLine.clear();
            }
            setKeyboardState(KeyboardState::Unlocked);
            event->accept();
            return;
        }
        if (hasSelection()) {
            clearSelection();
            event->accept();
            return;
        }
    }

    // Keyboard lock enforcement: when locked, only allow a few keys through
    if (m_keyboardState == KeyboardState::Locked) {
        // Allow only Attn (Ctrl+Escape) and SysReq — reject all other input
        // Attn and SysReq are processed via processKeyEvent -> processEncodedInput
        bool isAttn = (event->key() == Qt::Key_Escape && (event->modifiers() & Qt::ControlModifier));
        bool isSysReq = (event->key() == Qt::Key_SysReq);
        if (!isAttn && !isSysReq) {
            event->accept();
            return;
        }
    }
    if (m_keyboardState == KeyboardState::ErrorLocked) {
        // Only Error Reset (Escape, handled above), Attn, and Help are allowed
        bool isAttn = (event->key() == Qt::Key_Escape && (event->modifiers() & Qt::ControlModifier));
        bool isHelp = (event->key() == Qt::Key_F1 && (event->modifiers() & Qt::ControlModifier));
        if (!isAttn && !isHelp) {
            event->accept();
            return;
        }
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
            if (event->modifiers() & Qt::AltModifier) {
                // Erase Input: clear all non-protected fields, reset MDT, move to IC
                handleEraseInput();
            } else {
                handleDelete();
            }
            event->accept();
            return;
        case Qt::Key_Insert:
            m_insertMode = !m_insertMode;
            emit terminalStateChanged();
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

    // Determine if this is an AID key BEFORE encoding, since AID byte values
    // overlap with EBCDIC character codes (e.g. PF13-PF21 = 0xC1-0xC9 = 'A'-'I').
    int key = event->key();
    bool isAID = m_encoder->isPFKey(key) ||
                 key == Qt::Key_Return || key == Qt::Key_Enter ||
                 key == Qt::Key_PageUp || key == Qt::Key_PageDown ||
                 (key == Qt::Key_Escape && ctrlPressed) ||
                 key == Qt::Key_SysReq;

    QByteArray encoded =
        m_encoder->encodeKeyEvent(event, shiftPressed, ctrlPressed, altPressed);

    if (!encoded.isEmpty()) {
        processEncodedInput(encoded, isAID);
    }
}

void Q5250ScreenWidget::processEncodedInput(const QByteArray &data, bool isAID) {
    if (data.isEmpty() || !m_screenBuffer) {
        return;
    }

    uint8_t firstByte = static_cast<uint8_t>(data[0]);

    // AID detection is determined by the caller (processKeyEvent) based on the
    // original Qt key event, NOT by byte value.  AID byte values overlap with
    // EBCDIC character codes (e.g. PF13-PF21 0xC1-0xC9 == uppercase A-I).

    if (isAID) {
        // AID key: build field response and send to host
        emit inputReady(buildAIDResponse(firstByte));
        // Lock keyboard after sending AID — host must unlock via CC bytes
        setKeyboardState(KeyboardState::Locked);
    } else {
        // Regular EBCDIC character: write locally to screen buffer
        QPoint cursor = m_screenBuffer->cursorPosition();
        int row = cursor.y();
        int col = cursor.x();

        // Only write to unprotected positions
        if (!m_screenBuffer->isProtected(row, col)) {
            ScreenBuffer::Field field = m_screenBuffer->getField(row, col);

            // Field validation based on FFW shift type
            if (field.length > 0) {
                uint8_t eb = firstByte;
                switch (field.shiftType) {
                case 1: // Alpha only — reject digits (EBCDIC 0xF0-0xF9)
                    if (eb >= 0xF0 && eb <= 0xF9) return;
                    break;
                case 3: // Numeric shift — reject letters
                    if ((eb >= 0xC1 && eb <= 0xC9) || (eb >= 0xD1 && eb <= 0xD9) ||
                        (eb >= 0xE2 && eb <= 0xE9) || (eb >= 0x81 && eb <= 0x89) ||
                        (eb >= 0x91 && eb <= 0x99) || (eb >= 0xA2 && eb <= 0xA9))
                        return;
                    break;
                case 5: // Digits only — only allow 0xF0-0xF9
                    if (eb < 0xF0 || eb > 0xF9) return;
                    break;
                case 7: // Signed numeric — digits plus sign (0xF0-0xF9, 0x60 '-', 0x4E '+')
                    if (!((eb >= 0xF0 && eb <= 0xF9) || eb == 0x60 || eb == 0x4E)) return;
                    break;
                default:
                    break;
                }

                // Monocase: convert EBCDIC lowercase to uppercase
                if (field.monocase) {
                    if (firstByte >= 0x81 && firstByte <= 0x89)
                        firstByte = firstByte - 0x81 + 0xC1; // a-i -> A-I
                    else if (firstByte >= 0x91 && firstByte <= 0x99)
                        firstByte = firstByte - 0x91 + 0xD1; // j-r -> J-R
                    else if (firstByte >= 0xA2 && firstByte <= 0xA9)
                        firstByte = firstByte - 0xA2 + 0xE2; // s-z -> S-Z
                }
            }

            // Insert mode: shift field data right before writing
            if (m_insertMode && field.length > 0) {
                int fieldStart = field.startRow * m_screenBuffer->cols() + field.startCol;
                int fieldEnd = fieldStart + field.length;
                int curAddr = row * m_screenBuffer->cols() + col;

                // Check if last position has data — insert would overflow
                int lastAddr = fieldEnd - 1;
                int lastR = lastAddr / m_screenBuffer->cols();
                int lastC = lastAddr % m_screenBuffer->cols();
                uint8_t lastCh = m_screenBuffer->character(lastR, lastC);
                if (lastCh != 0x40 && lastCh != 0x00) {
                    // Field full — cannot insert, reject keystroke
                    return;
                }

                // Shift right from end of field to cursor position
                for (int addr = fieldEnd - 1; addr > curAddr; --addr) {
                    int srcR = (addr - 1) / m_screenBuffer->cols();
                    int srcC = (addr - 1) % m_screenBuffer->cols();
                    int dstR = addr / m_screenBuffer->cols();
                    int dstC = addr % m_screenBuffer->cols();
                    uint8_t ch = m_screenBuffer->character(srcR, srcC);
                    m_screenBuffer->writeChar(dstR, dstC, ch);
                }
            }

            m_screenBuffer->writeChar(row, col, firstByte);
            m_screenBuffer->markFieldModified(row, col);

            // Field boundary-aware cursor advancement
            if (field.length > 0) {
                int fieldStart = field.startRow * m_screenBuffer->cols() + field.startCol;
                int fieldEnd = fieldStart + field.length;
                int curAddr = row * m_screenBuffer->cols() + col;

                if (curAddr + 1 >= fieldEnd) {
                    // Reached last position of field
                    if (field.autoEnter) {
                        // Auto-enter: send with Auto Enter AID (0x3F per spec)
                        emit inputReady(buildAIDResponse(0x3F));
                        setKeyboardState(KeyboardState::Locked);
                    } else if (field.fieldExitReq) {
                        // FER: cursor stays on last character;
                        // operator must press Field Exit key to leave
                    } else {
                        // Normal field: advance to next input field
                        rightAdjustField(row, col);
                        ScreenBuffer::Field nextField = findNextField(row, col);
                        if (nextField.length > 0) {
                            m_screenBuffer->setCursorPosition(nextField.startRow, nextField.startCol);
                        }
                    }
                    update();
                    return;
                }
            }

            // Normal advancement within field
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

    // Right-adjust current field on exit
    m_cursorPos = m_screenBuffer->cursorPosition();
    rightAdjustField(m_cursorPos.y(), m_cursorPos.x());

    ScreenBuffer::Field nextField = findNextField(m_cursorPos.y(), m_cursorPos.x());
    if (nextField.length > 0) {
        moveCursor(nextField.startRow, nextField.startCol);
    }
}

void Q5250ScreenWidget::moveToPreviousField() {
    if (!m_screenBuffer) {
        return;
    }

    // Right-adjust current field on exit
    m_cursorPos = m_screenBuffer->cursorPosition();
    rightAdjustField(m_cursorPos.y(), m_cursorPos.x());

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
    const auto &fields = m_screenBuffer->fields();
    if (fields.isEmpty()) {
        return ScreenBuffer::Field();
    }
    int cols = m_screenBuffer->cols();
    int curAddr = startRow * cols + startCol;

    // First pass: find first eligible field strictly after cursor position
    ScreenBuffer::Field best;
    int bestAddr = INT_MAX;
    for (const auto &f : fields) {
        if (f.length <= 0 || f.protected_field || f.bypass) continue;
        int fAddr = f.startRow * cols + f.startCol;
        if (fAddr > curAddr && fAddr < bestAddr) {
            best = f;
            bestAddr = fAddr;
        }
    }
    if (best.length > 0) return best;

    // Wrap around: find the first eligible field from the top of the screen
    bestAddr = INT_MAX;
    for (const auto &f : fields) {
        if (f.length <= 0 || f.protected_field || f.bypass) continue;
        int fAddr = f.startRow * cols + f.startCol;
        if (fAddr < bestAddr) {
            best = f;
            bestAddr = fAddr;
        }
    }
    return best;
}

ScreenBuffer::Field Q5250ScreenWidget::findPreviousField(int startRow, int startCol) const {
    if (!m_screenBuffer) {
        return ScreenBuffer::Field();
    }
    const auto &fields = m_screenBuffer->fields();
    if (fields.isEmpty()) {
        return ScreenBuffer::Field();
    }
    int cols = m_screenBuffer->cols();
    int curAddr = startRow * cols + startCol;

    // First pass: find the closest eligible field strictly before cursor position
    ScreenBuffer::Field best;
    int bestAddr = -1;
    for (const auto &f : fields) {
        if (f.length <= 0 || f.protected_field || f.bypass) continue;
        int fAddr = f.startRow * cols + f.startCol;
        if (fAddr < curAddr && fAddr > bestAddr) {
            best = f;
            bestAddr = fAddr;
        }
    }
    if (best.length > 0) return best;

    // Wrap around: find the last eligible field on screen
    bestAddr = -1;
    for (const auto &f : fields) {
        if (f.length <= 0 || f.protected_field || f.bypass) continue;
        int fAddr = f.startRow * cols + f.startCol;
        if (fAddr > bestAddr) {
            best = f;
            bestAddr = fAddr;
        }
    }
    return best;
}

void Q5250ScreenWidget::handleEraseInput() {
    if (!m_screenBuffer) return;

    // Clear all non-protected fields to nulls, reset MDT flags
    const auto &fields = m_screenBuffer->fields();
    for (int fi = 0; fi < fields.size(); ++fi) {
        const auto &field = fields[fi];
        if (!field.protected_field) {
            int addr = field.startRow * m_screenBuffer->cols() + field.startCol;
            for (int j = 0; j < field.length; ++j) {
                int r = (addr + j) / m_screenBuffer->cols();
                int c = (addr + j) % m_screenBuffer->cols();
                m_screenBuffer->writeChar(r, c, 0x00); // Null
            }
        }
    }

    // Move cursor to IC address
    m_screenBuffer->setCursorPosition(m_icRow, m_icCol);
    update();
}

void Q5250ScreenWidget::rightAdjustField(int row, int col) {
    if (!m_screenBuffer) return;
    ScreenBuffer::Field field = m_screenBuffer->getField(row, col);
    if (field.length <= 0 || field.rightAdjust == 0) return;

    int fieldStart = field.startRow * m_screenBuffer->cols() + field.startCol;
    int fieldEnd = fieldStart + field.length;

    // Find last non-blank character
    int lastData = fieldStart - 1;
    for (int addr = fieldEnd - 1; addr >= fieldStart; --addr) {
        int r = addr / m_screenBuffer->cols();
        int c = addr % m_screenBuffer->cols();
        uint8_t ch = m_screenBuffer->character(r, c);
        if (ch != 0x40 && ch != 0x00) {
            lastData = addr;
            break;
        }
    }

    int dataLen = lastData - fieldStart + 1;
    if (dataLen <= 0 || dataLen >= field.length) return; // Nothing to adjust or already full

    // Fill character: 0x00 (null) for type 5 (zero-fill), 0x40 (blank) otherwise
    uint8_t fillChar = (field.rightAdjust == 5) ? 0xF0 : 0x40; // type 5 = zero-fill (EBCDIC '0')

    // Shift data to the right
    int shift = field.length - dataLen;
    for (int addr = fieldEnd - 1; addr >= fieldStart; --addr) {
        int srcAddr = addr - shift;
        int r = addr / m_screenBuffer->cols();
        int c = addr % m_screenBuffer->cols();
        if (srcAddr >= fieldStart) {
            int srcR = srcAddr / m_screenBuffer->cols();
            int srcC = srcAddr % m_screenBuffer->cols();
            m_screenBuffer->writeChar(r, c, m_screenBuffer->character(srcR, srcC));
        } else {
            m_screenBuffer->writeChar(r, c, fillChar);
        }
    }
    m_screenBuffer->markFieldModified(field.startRow, field.startCol);
    update();
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
