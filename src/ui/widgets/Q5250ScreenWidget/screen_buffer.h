#pragma once

#include <QByteArray>
#include <QObject>
#include <QPoint>
#include <QVector>
#include <cstdint>

namespace ui::widgets {

// Screen cell attributes
struct CellAttributes {
    uint8_t color : 4;        // Color code (0-15)
    bool reverse : 1;         // Reverse video
    bool blink : 1;           // Blinking
    bool underline : 1;       // Underline
    bool protected_field : 1; // Protected field (read-only)
    bool modified : 1;        // Field has been modified
    bool nonDisplay : 1;      // Non-display (hidden, e.g. password fields)
    bool colSep : 1;          // Column separator (vertical line on left edge)

    CellAttributes()
        : color(2), reverse(false), blink(false), underline(false),
          protected_field(false), modified(false), nonDisplay(false), colSep(false) {}
};

// Screen cell containing character and attributes
struct ScreenCell {
    uint8_t character; // EBCDIC character
    CellAttributes attributes;

    ScreenCell() : character(0x40) {} // Default to EBCDIC space
};

// Screen buffer for TN5250 display
class ScreenBuffer : public QObject {
    Q_OBJECT

  public:
    explicit ScreenBuffer(int rows = 24, int cols = 80, QObject *parent = nullptr);
    ~ScreenBuffer();

    // Dimensions
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    void resize(int rows, int cols);

    // Cell access
    ScreenCell &cell(int row, int col);
    const ScreenCell &cell(int row, int col) const;
    uint8_t character(int row, int col) const;
    CellAttributes attributes(int row, int col) const;

    // Cursor management
    QPoint cursorPosition() const { return m_cursorPos; }
    void setCursorPosition(int row, int col);
    void setCursorPosition(const QPoint &pos);
    bool isCursorVisible() const { return m_cursorVisible; }
    void setCursorVisible(bool visible);

    // Field management
    struct Field {
        int startRow = 0;
        int startCol = 0;
        int length = 0;
        bool protected_field = false;
        bool modified = false;

        // FFW (Field Format Word) — parsed from SF order
        uint8_t ffw1 = 0;           // Raw FFW byte 1
        uint8_t ffw2 = 0;           // Raw FFW byte 2
        uint8_t shiftType = 0;      // FFW1 bits 5-7: field shift/data type
        bool bypass = false;        // FFW1 bit 2: tab skips this field
        bool autoEnter = false;     // FFW2 bit 0: auto-enter when field full
        bool fieldExitReq = false;  // FFW2 bit 1: field exit required
        bool monocase = false;      // FFW2 bit 2: uppercase only
        bool mandatoryEnter = false;// FFW2 bit 4: mandatory enter
        uint8_t rightAdjust = 0;    // FFW2 bits 5-7: right-adjust type
    };

    void setField(int row, int col, int length, bool protected_field);
    void setFieldFFW(int row, int col, uint8_t ffw1, uint8_t ffw2);
    Field getField(int row, int col) const;
    bool isInField(int row, int col) const;
    bool isProtected(int row, int col) const;
    const QVector<Field> &fields() const { return m_fields; }
    QVector<Field> &mutableFields() { return m_fields; }
    void resetAllMDTFlags();
    QVector<Field> getModifiedFields() const;
    QByteArray getFieldData(const Field &field) const;
    void markFieldModified(int row, int col);

    // Screen operations
    void clear();
    void clearRow(int row);
    void clearField(int row, int col);
    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);
    void scrollRegion(int topRow, int botRow, int lines, bool up);
    void clearFields();

    // Save/restore entire screen state
    struct SavedState {
        QVector<ScreenCell> buffer;
        QVector<Field> fields;
        QPoint cursorPos;
        int rows;
        int cols;
    };
    SavedState saveState() const;
    void restoreState(const SavedState &state);

    // Write operations
    void writeChar(int row, int col, uint8_t ch);
    void writeChar(int row, int col, uint8_t ch, const CellAttributes &attr);
    void writeString(int row, int col, const QByteArray &data, const CellAttributes &attr = CellAttributes());
    void eraseWrite(int row, int col, int length);
    void eraseWriteAlternate(int row, int col, int length);

    // Attribute operations
    void setAttributes(int row, int col, const CellAttributes &attr);
    void setColor(int row, int col, uint8_t color);
    void setReverse(int row, int col, bool reverse);
    void setBlink(int row, int col, bool blink);
    void setUnderline(int row, int col, bool underline);

    // Notify current cursor position to listeners even if unchanged
    void notifyCursor() { emit cursorMoved(m_cursorPos); }

  signals:
    void screenChanged();
    void cursorMoved(const QPoint &pos);
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

} // namespace ui::widgets
