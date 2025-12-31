#include "screen_buffer.h"
#include <QDebug>

namespace display {

ScreenBuffer::ScreenBuffer(int rows, int cols, QObject* parent)
    : QObject(parent)
    , m_rows(rows)
    , m_cols(cols)
    , m_cursorPos(0, 0)
    , m_cursorVisible(true)
{
    m_buffer.resize(m_rows * m_cols);
    clear();
}

ScreenBuffer::~ScreenBuffer() {
}

void ScreenBuffer::resize(int rows, int cols) {
    m_rows = rows;
    m_cols = cols;
    m_buffer.resize(m_rows * m_cols);
    clear();
    emit screenChanged();
}

int ScreenBuffer::index(int row, int col) const {
    return row * m_cols + col;
}

bool ScreenBuffer::isValidPosition(int row, int col) const {
    return row >= 0 && row < m_rows && col >= 0 && col < m_cols;
}

ScreenCell& ScreenBuffer::cell(int row, int col) {
    Q_ASSERT(isValidPosition(row, col));
    return m_buffer[index(row, col)];
}

const ScreenCell& ScreenBuffer::cell(int row, int col) const {
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
            emit cursorMoved(m_cursorPos);
        }
    }
}

void ScreenBuffer::setCursorPosition(const QPoint& pos) {
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
    
    // Remove any existing field at this position
    for (int i = m_fields.size() - 1; i >= 0; --i) {
        Field& f = m_fields[i];
        if (f.startRow == row && f.startCol <= col && col < f.startCol + f.length) {
            m_fields.removeAt(i);
        }
    }
    
    // Add new field
    Field field;
    field.startRow = row;
    field.startCol = col;
    field.length = qMin(length, m_cols - col);
    field.protected_field = protected_field;
    field.modified = false;
    m_fields.append(field);
    
    // Update cell attributes
    for (int i = 0; i < field.length && col + i < m_cols; ++i) {
        cell(row, col + i).attributes.protected_field = protected_field;
    }
    
    emit fieldChanged(row, col);
}

ScreenBuffer::Field ScreenBuffer::getField(int row, int col) const {
    for (const Field& field : m_fields) {
        if (field.startRow == row && field.startCol <= col && col < field.startCol + field.length) {
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
    for (ScreenCell& cell : m_buffer) {
        cell.character = 0x40; // EBCDIC space
        cell.attributes = CellAttributes();
    }
    m_fields.clear();
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
        for (int i = 0; i < field.length; ++i) {
            int c = field.startCol + i;
            if (c < m_cols) {
                cell(field.startRow, c).character = 0x40;
            }
        }
        emit screenChanged();
    }
}

void ScreenBuffer::scrollUp(int lines) {
    if (lines <= 0 || lines >= m_rows) {
        return;
    }
    
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
    for (Field& field : m_fields) {
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
    for (Field& field : m_fields) {
        field.startRow += lines;
        if (field.startRow >= m_rows) {
            field.startRow = m_rows - 1;
        }
    }
    
    emit screenChanged();
}

void ScreenBuffer::writeChar(int row, int col, uint8_t ch, const CellAttributes& attr) {
    if (!isValidPosition(row, col)) {
        return;
    }
    
    ScreenCell& c = cell(row, col);
    c.character = ch;
    c.attributes = attr;
    
    emit screenChanged();
}

void ScreenBuffer::writeString(int row, int col, const QByteArray& data, const CellAttributes& attr) {
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

void ScreenBuffer::setAttributes(int row, int col, const CellAttributes& attr) {
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

} // namespace display

