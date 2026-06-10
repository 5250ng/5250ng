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

#include "screen_buffer.h"
#include "logger/logger.h"
#include <QDebug>

namespace ui::widgets {

ScreenBuffer::ScreenBuffer(int rows, int cols, QObject *parent) : QObject(parent), m_rows(rows), m_cols(cols), m_cursorPos(0, 0), m_cursorVisible(true) {
    m_buffer.resize(m_rows * m_cols);
    clear();
}

ScreenBuffer::~ScreenBuffer() {}

void ScreenBuffer::resize(int rows, int cols) {
    LOG_DEBUG(QString("[ScreenBuffer] resize: %1x%2 -> %3x%4")
        .arg(m_rows).arg(m_cols).arg(rows).arg(cols));
    m_rows = rows;
    m_cols = cols;
    m_buffer.resize(m_rows * m_cols);
    clear();
    emit screenChanged();
}

int ScreenBuffer::index(int row, int col) const { return row * m_cols + col; }

bool ScreenBuffer::isValidPosition(int row, int col) const {
    return row >= 0 && row < m_rows && col >= 0 && col < m_cols;
}

ScreenCell &ScreenBuffer::cell(int row, int col) {
    Q_ASSERT(isValidPosition(row, col));
    return m_buffer[index(row, col)];
}

const ScreenCell &ScreenBuffer::cell(int row, int col) const {
    Q_ASSERT(isValidPosition(row, col));
    return m_buffer[index(row, col)];
}

uint8_t ScreenBuffer::character(int row, int col) const {
    if (!isValidPosition(row, col)) {
        return 0x40; // EBCDIC space
    }
    return m_buffer[index(row, col)].character;
}

CellAttributes ScreenBuffer::attributes(int row, int col) const {
    if (!isValidPosition(row, col)) {
        return CellAttributes();
    }
    return m_buffer[index(row, col)].attributes;
}

void ScreenBuffer::setCursorPosition(int row, int col) {
    if (isValidPosition(row, col)) {
        QPoint oldPos = m_cursorPos;
        m_cursorPos = QPoint(col, row);
        if (oldPos != m_cursorPos) {
            LOG_DEBUG(QString("[ScreenBuffer] setCursorPosition: (%1,%2) -> (%3,%4)")
                .arg(oldPos.y()).arg(oldPos.x()).arg(row).arg(col));
            emit cursorMoved(m_cursorPos);
        }
    }
}

void ScreenBuffer::setCursorPosition(const QPoint &pos) {
    setCursorPosition(pos.y(), pos.x());
}

void ScreenBuffer::setCursorVisible(bool visible) {
    if (m_cursorVisible != visible) {
        m_cursorVisible = visible;
        emit screenChanged();
    }
}

void ScreenBuffer::setField(int row, int col, int length, bool protected_field) {
    if (!isValidPosition(row, col)) {
        return;
    }

    // Remove any existing field that overlaps this position
    int newStart = row * m_cols + col;
    int newEnd = newStart + length; // one past end
    for (int i = m_fields.size() - 1; i >= 0; --i) {
        Field &f = m_fields[i];
        int fStart = f.startRow * m_cols + f.startCol;
        int fEnd = fStart + f.length;
        if (fStart < newEnd && fEnd > newStart) {
            m_fields.removeAt(i);
        }
    }

    // Add new field (may span multiple rows)
    Field field;
    field.startRow = row;
    field.startCol = col;
    field.length = length;
    field.protected_field = protected_field;
    field.modified = false;
    m_fields.append(field);
    LOG_DEBUG(QString("[ScreenBuffer] setField: row=%1 col=%2 len=%3 protected=%4 (total fields=%5)")
        .arg(row).arg(col).arg(length).arg(protected_field).arg(m_fields.size()));

    // Update cell attributes for all cells in the field (wrapping rows)
    int addr = row * m_cols + col;
    int totalCells = m_rows * m_cols;
    for (int i = 0; i < length && addr + i < totalCells; ++i) {
        int r = (addr + i) / m_cols;
        int c = (addr + i) % m_cols;
        m_buffer[index(r, c)].attributes.protected_field = protected_field;
    }

    emit fieldChanged(row, col);
}

void ScreenBuffer::setFieldFFW(int row, int col, uint8_t ffw1, uint8_t ffw2) {
    LOG_DEBUG(QString("[ScreenBuffer] setFieldFFW: row=%1 col=%2 ffw1=0x%3 ffw2=0x%4")
        .arg(row).arg(col).arg(ffw1, 2, 16, QChar('0')).arg(ffw2, 2, 16, QChar('0')));
    int addr = row * m_cols + col;
    for (Field &field : m_fields) {
        int fStart = field.startRow * m_cols + field.startCol;
        if (fStart == addr) {
            field.ffw1 = ffw1;
            field.ffw2 = ffw2;
            // FFW1 (SA21-9247-6 p.2-68): IBM bit 0 = MSB = 0x80
            //   Bits 0-1 (0xC0): Must be 01 (FFW marker)
            //   Bit  2   (0x20): Bypass
            //   Bit  3   (0x10): Dup enable
            //   Bit  4   (0x08): MDT
            //   Bits 5-7 (0x07): Shift/data type
            field.shiftType = ffw1 & 0x07;
            field.bypass = (ffw1 & 0x20) != 0;
            // FFW2 (SA21-9247-6 p.2-69): IBM bit 0 = MSB = 0x80
            //   Bit  0   (0x80): Auto enter
            //   Bit  1   (0x40): Field exit required
            //   Bit  2   (0x20): Monocase
            //   Bit  3   (0x10): Reserved
            //   Bit  4   (0x08): Mandatory enter
            //   Bits 5-7 (0x07): Right-adjust / mandatory fill
            field.autoEnter = (ffw2 & 0x80) != 0;
            field.fieldExitReq = (ffw2 & 0x40) != 0;
            field.monocase = (ffw2 & 0x20) != 0;
            field.mandatoryEnter = (ffw2 & 0x08) != 0;
            field.rightAdjust = ffw2 & 0x07;
            return;
        }
    }
}

ScreenBuffer::Field ScreenBuffer::getField(int row, int col) const {
    int addr = row * m_cols + col;
    for (const Field &field : m_fields) {
        int fStart = field.startRow * m_cols + field.startCol;
        int fEnd = fStart + field.length;
        if (addr >= fStart && addr < fEnd) {
            return field;
        }
    }
    return Field(); // Empty field
}

bool ScreenBuffer::isInField(int row, int col) const {
    return getField(row, col).length > 0;
}

bool ScreenBuffer::isProtected(int row, int col) const {
    if (!isValidPosition(row, col)) {
        return true; // Out of bounds is protected
    }
    return m_buffer[index(row, col)].attributes.protected_field;
}

void ScreenBuffer::clear() {
    LOG_DEBUG(QString("[ScreenBuffer] clear: clearing %1x%2 buffer, %3 fields")
        .arg(m_rows).arg(m_cols).arg(m_fields.size()));
    for (ScreenCell &cell : m_buffer) {
        cell.character = 0x40; // EBCDIC space
        cell.attributes = CellAttributes();
    }
    m_fields.clear();
    // Reset cursor to top-left (row 0, col 0)
    setCursorPosition(0, 0);
    emit screenChanged();
}

void ScreenBuffer::clearRow(int row) {
    if (!isValidPosition(row, 0)) {
        return;
    }

    for (int col = 0; col < m_cols; ++col) {
        cell(row, col).character = 0x40;
        cell(row, col).attributes = CellAttributes();
    }

    // Remove fields in this row
    for (int i = m_fields.size() - 1; i >= 0; --i) {
        if (m_fields[i].startRow == row) {
            m_fields.removeAt(i);
        }
    }

    emit screenChanged();
}

void ScreenBuffer::clearField(int row, int col) {
    Field field = getField(row, col);
    if (field.length > 0) {
        int addr = field.startRow * m_cols + field.startCol;
        int totalCells = m_rows * m_cols;
        for (int i = 0; i < field.length && addr + i < totalCells; ++i) {
            int r = (addr + i) / m_cols;
            int c = (addr + i) % m_cols;
            m_buffer[index(r, c)].character = 0x40;
        }
        emit screenChanged();
    }
}

void ScreenBuffer::scrollUp(int lines) {
    if (lines <= 0 || lines >= m_rows) {
        return;
    }
    LOG_DEBUG(QString("[ScreenBuffer] scrollUp: %1 lines").arg(lines));

    // Move lines up
    for (int row = 0; row < m_rows - lines; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            m_buffer[index(row, col)] = m_buffer[index(row + lines, col)];
        }
    }

    // Clear bottom lines
    for (int row = m_rows - lines; row < m_rows; ++row) {
        clearRow(row);
    }

    // Update field positions
    for (Field &field : m_fields) {
        field.startRow -= lines;
        if (field.startRow < 0) {
            field.startRow = 0;
        }
    }

    emit screenChanged();
}

void ScreenBuffer::scrollDown(int lines) {
    if (lines <= 0 || lines >= m_rows) {
        return;
    }
    LOG_DEBUG(QString("[ScreenBuffer] scrollDown: %1 lines").arg(lines));

    // Move lines down
    for (int row = m_rows - 1; row >= lines; --row) {
        for (int col = 0; col < m_cols; ++col) {
            m_buffer[index(row, col)] = m_buffer[index(row - lines, col)];
        }
    }

    // Clear top lines
    for (int row = 0; row < lines; ++row) {
        clearRow(row);
    }

    // Update field positions
    for (Field &field : m_fields) {
        field.startRow += lines;
        if (field.startRow >= m_rows) {
            field.startRow = m_rows - 1;
        }
    }

    emit screenChanged();
}

void ScreenBuffer::writeChar(int row, int col, uint8_t ch) {
    if (!isValidPosition(row, col)) {
        return;
    }

    cell(row, col).character = ch;

    emit screenChanged();
}

void ScreenBuffer::writeChar(int row, int col, uint8_t ch, const CellAttributes &attr) {
    if (!isValidPosition(row, col)) {
        return;
    }

    ScreenCell &c = cell(row, col);
    c.character = ch;
    c.attributes = attr;

    emit screenChanged();
}

void ScreenBuffer::writeString(int row, int col, const QByteArray &data, const CellAttributes &attr) {
    if (!isValidPosition(row, col)) {
        return;
    }

    int pos = 0;
    int currentRow = row;
    int currentCol = col;

    while (pos < data.size() && isValidPosition(currentRow, currentCol)) {
        writeChar(currentRow, currentCol, static_cast<uint8_t>(data[pos]), attr);
        pos++;
        currentCol++;
        if (currentCol >= m_cols) {
            currentCol = 0;
            currentRow++;
        }
    }
}

void ScreenBuffer::eraseWrite(int row, int col, int length) {
    if (!isValidPosition(row, col)) {
        return;
    }

    for (int i = 0; i < length && col + i < m_cols; ++i) {
        cell(row, col + i).character = 0x40; // EBCDIC space
    }

    emit screenChanged();
}

void ScreenBuffer::eraseWriteAlternate(int row, int col, int length) {
    eraseWrite(row, col, length);
}

void ScreenBuffer::setAttributes(int row, int col, const CellAttributes &attr) {
    if (!isValidPosition(row, col)) {
        return;
    }

    cell(row, col).attributes = attr;
    emit screenChanged();
}

void ScreenBuffer::setColor(int row, int col, uint8_t color) {
    if (!isValidPosition(row, col)) {
        return;
    }

    cell(row, col).attributes.color = color & 0x0F;
    emit screenChanged();
}

void ScreenBuffer::setReverse(int row, int col, bool reverse) {
    if (!isValidPosition(row, col)) {
        return;
    }

    cell(row, col).attributes.reverse = reverse;
    emit screenChanged();
}

void ScreenBuffer::setBlink(int row, int col, bool blink) {
    if (!isValidPosition(row, col)) {
        return;
    }

    cell(row, col).attributes.blink = blink;
    emit screenChanged();
}

void ScreenBuffer::setUnderline(int row, int col, bool underline) {
    if (!isValidPosition(row, col)) {
        return;
    }

    cell(row, col).attributes.underline = underline;
    emit screenChanged();
}

void ScreenBuffer::updateField(int row, int col) {
    Field field = getField(row, col);
    if (field.length > 0) {
        field.modified = true;
        for (int i = 0; i < m_fields.size(); ++i) {
            if (m_fields[i].startRow == field.startRow &&
                m_fields[i].startCol == field.startCol) {
                m_fields[i] = field;
                break;
            }
        }
    }
}

void ScreenBuffer::markFieldModified(int row, int col) {
    for (int i = 0; i < m_fields.size(); ++i) {
        int fStart = m_fields[i].startRow * m_cols + m_fields[i].startCol;
        int fEnd = fStart + m_fields[i].length;
        int addr = row * m_cols + col;
        if (addr >= fStart && addr < fEnd) {
            if (!m_fields[i].modified) {
                LOG_DEBUG(QString("[ScreenBuffer] markFieldModified: field at (%1,%2) len=%3 marked MDT")
                    .arg(m_fields[i].startRow).arg(m_fields[i].startCol).arg(m_fields[i].length));
            }
            m_fields[i].modified = true;
            return;
        }
    }
}

QVector<ScreenBuffer::Field> ScreenBuffer::getModifiedFields() const {
    QVector<Field> result;
    for (const Field &f : m_fields) {
        if (f.modified && !f.protected_field) {
            result.append(f);
        }
    }
    LOG_DEBUG(QString("[ScreenBuffer] getModifiedFields: %1 modified out of %2 total")
        .arg(result.size()).arg(m_fields.size()));
    return result;
}

QByteArray ScreenBuffer::getFieldData(const Field &field) const {
    QByteArray data;
    int addr = field.startRow * m_cols + field.startCol;
    int totalCells = m_rows * m_cols;
    for (int i = 0; i < field.length && addr + i < totalCells; ++i) {
        int r = (addr + i) / m_cols;
        int c = (addr + i) % m_cols;
        data.append(static_cast<char>(m_buffer[index(r, c)].character));
    }
    return data;
}

void ScreenBuffer::scrollRegion(int topRow, int botRow, int lines, bool up) {
    if (topRow < 0) topRow = 0;
    if (botRow >= m_rows) botRow = m_rows - 1;
    if (topRow > botRow || lines <= 0) return;
    // A line count >= the region height shifts everything out of the window:
    // clamp so the clear loops below stay inside [topRow, botRow]. Without
    // this, a host-controlled Roll order with lineCount up to 31 makes the
    // up-branch clear loop start at a negative row (write before m_buffer)
    // and the down-branch clear loop run past botRow (write after m_buffer).
    const int regionHeight = botRow - topRow + 1;
    if (lines > regionHeight) lines = regionHeight;
    LOG_DEBUG(QString("[ScreenBuffer] scrollRegion: top=%1 bot=%2 lines=%3 dir=%4")
        .arg(topRow).arg(botRow).arg(lines).arg(up ? "up" : "down"));

    if (up) {
        // Scroll up: move rows upward, clear vacated rows at bottom
        for (int row = topRow; row <= botRow - lines; ++row) {
            for (int col = 0; col < m_cols; ++col) {
                m_buffer[index(row, col)] = m_buffer[index(row + lines, col)];
            }
        }
        for (int row = botRow - lines + 1; row <= botRow; ++row) {
            for (int col = 0; col < m_cols; ++col) {
                m_buffer[index(row, col)] = ScreenCell();
            }
        }
    } else {
        // Scroll down: move rows downward, clear vacated rows at top
        for (int row = botRow; row >= topRow + lines; --row) {
            for (int col = 0; col < m_cols; ++col) {
                m_buffer[index(row, col)] = m_buffer[index(row - lines, col)];
            }
        }
        for (int row = topRow; row < topRow + lines; ++row) {
            for (int col = 0; col < m_cols; ++col) {
                m_buffer[index(row, col)] = ScreenCell();
            }
        }
    }
    emit screenChanged();
}

void ScreenBuffer::resetAllMDTFlags() {
    int count = 0;
    for (auto &field : m_fields) {
        if (field.modified) count++;
        field.modified = false;
    }
    LOG_DEBUG(QString("[ScreenBuffer] resetAllMDTFlags: cleared %1 modified flags out of %2 fields")
        .arg(count).arg(m_fields.size()));
}

void ScreenBuffer::clearFields() {
    LOG_DEBUG(QString("[ScreenBuffer] clearFields: removing %1 fields").arg(m_fields.size()));
    m_fields.clear();
}

ScreenBuffer::SavedState ScreenBuffer::saveState() const {
    SavedState state;
    state.buffer = m_buffer;
    state.fields = m_fields;
    state.cursorPos = m_cursorPos;
    state.rows = m_rows;
    state.cols = m_cols;
    return state;
}

void ScreenBuffer::restoreState(const SavedState &state) {
    LOG_DEBUG(QString("[ScreenBuffer] restoreState: %1x%2 buffer, %3 fields, cursor=(%4,%5)")
        .arg(state.rows).arg(state.cols).arg(state.fields.size())
        .arg(state.cursorPos.y()).arg(state.cursorPos.x()));
    if (state.rows != m_rows || state.cols != m_cols) {
        resize(state.rows, state.cols);
    }
    m_buffer = state.buffer;
    m_fields = state.fields;
    m_cursorPos = state.cursorPos;
    emit screenChanged();
    emit cursorMoved(m_cursorPos);
}

} // namespace ui::widgets
