#pragma once

#include <QObject>
#include <QPoint>
#include <QByteArray>
#include <QVector>
#include <cstdint>

namespace display {

// Screen cell attributes
struct CellAttributes {
    uint8_t color : 4;           // Color code (0-15)
    bool reverse : 1;            // Reverse video
    bool blink : 1;              // Blinking
    bool underline : 1;          // Underline
    bool protected_field : 1;    // Protected field (read-only)
    bool modified : 1;            // Field has been modified
    
    CellAttributes() : color(7), reverse(false), blink(false), 
                      underline(false), protected_field(false), modified(false) {}
};

// Screen cell containing character and attributes
struct ScreenCell {
    uint8_t character;           // EBCDIC character
    CellAttributes attributes;
    
    ScreenCell() : character(0x40) {} // Default to EBCDIC space
};

// Screen buffer for TN5250 display
class ScreenBuffer : public QObject {
    Q_OBJECT

public:
    explicit ScreenBuffer(int rows = 24, int cols = 80, QObject* parent = nullptr);
    ~ScreenBuffer();

    // Dimensions
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    void resize(int rows, int cols);

    // Cell access
    ScreenCell& cell(int row, int col);
    const ScreenCell& cell(int row, int col) const;
    uint8_t character(int row, int col) const;
    CellAttributes attributes(int row, int col) const;

    // Cursor management
    QPoint cursorPosition() const { return m_cursorPos; }
    void setCursorPosition(int row, int col);
    void setCursorPosition(const QPoint& pos);
    bool isCursorVisible() const { return m_cursorVisible; }
    void setCursorVisible(bool visible);

    // Field management
    struct Field {
        int startRow;
        int startCol;
        int length;
        bool protected_field;
        bool modified;
    };
    
    void setField(int row, int col, int length, bool protected_field);
    Field getField(int row, int col) const;
    bool isInField(int row, int col) const;
    bool isProtected(int row, int col) const;

    // Screen operations
    void clear();
    void clearRow(int row);
    void clearField(int row, int col);
    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);
    
    // Write operations
    void writeChar(int row, int col, uint8_t ch);
    void writeChar(int row, int col, uint8_t ch, const CellAttributes& attr);
    void writeString(int row, int col, const QByteArray& data, const CellAttributes& attr = CellAttributes());
    void eraseWrite(int row, int col, int length);
    void eraseWriteAlternate(int row, int col, int length);

    // Attribute operations
    void setAttributes(int row, int col, const CellAttributes& attr);
    void setColor(int row, int col, uint8_t color);
    void setReverse(int row, int col, bool reverse);
    void setBlink(int row, int col, bool blink);
    void setUnderline(int row, int col, bool underline);

signals:
    void screenChanged();
    void cursorMoved(const QPoint& pos);
    void fieldChanged(int row, int col);

private:
    int m_rows;
    int m_cols;
    QVector<ScreenCell> m_buffer;
    QPoint m_cursorPos;
    bool m_cursorVisible;
    QVector<Field> m_fields;

    int index(int row, int col) const;
    bool isValidPosition(int row, int col) const;
    void updateField(int row, int col);
};

} // namespace display

