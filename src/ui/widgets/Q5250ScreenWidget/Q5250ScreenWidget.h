#pragma once

#include "core/ebcdic.h"
#include "core/keyboard_encoder.h"
#include "screen_buffer.h"
#include <QClipboard>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>
#include "Q5250Cursor.h"

namespace ui::widgets {

// 5250 keyboard state machine (per IBM SA21-9247-6 "States and Modes")
enum class KeyboardState : uint8_t {
    Unlocked,       // Normal data entry (data mode, insert mode, command mode)
    Locked,         // After AID key press or host command; waiting for host unlock
    ErrorLocked,    // Operator error or Write Error Code; only Error Reset/Help/Attn work
    SystemRequest   // System request state; keyboard unlocked for SysReq message entry
};

// Custom QWidget for TN5250 display rendering
class Q5250ScreenWidget : public QWidget {
    Q_OBJECT

  public:
    explicit Q5250ScreenWidget(QWidget *parent = nullptr);
    ~Q5250ScreenWidget();

    // Cursor rules overlay (crosshair lines)
    void setShowCursorRules(bool enabled);
    void toggleCursorRules();
    bool showCursorRules() const { return m_showCursorRules; }

    // Field protection overlay
    void setShowFieldProtection(bool enabled);
    void toggleFieldProtection();
    bool showFieldProtection() const { return m_showFieldProtection; }

    // Input fields overlay
    void setShowInputFields(bool enabled);
    void toggleInputFields();
    bool showInputFields() const { return m_showInputFields; }

    // Screen buffer access
    ScreenBuffer *screenBuffer() { return m_screenBuffer; }
    const ScreenBuffer *screenBuffer() const { return m_screenBuffer; }

    // Display configuration
    void setScreenSize(int rows, int cols);
    int screenRows() const { return m_screenBuffer->rows(); }
    int screenCols() const { return m_screenBuffer->cols(); }

    // Font configuration
    void setFont(const QFont &font);
    QFont font() const { return m_font; }

    // Color scheme
    void setColorScheme(const QVector<QColor> &colors);
    QColor backgroundColor() const { return m_bgColor; }
    QColor foregroundColor() const { return m_fgColor; }
    void setBackgroundColor(const QColor &color);
    void setForegroundColor(const QColor &color);

    // Cursor
    void setCursorBlinkRate(int msec);
    int cursorBlinkRate() const { return m_cursorBlinkRate; }
    void setCursorEnabled(bool enabled);
    bool isCursorEnabled() const { return m_cursorEnabled; }
    void setSelectionEnabled(bool enabled) { m_selectionEnabled = enabled; update(); }
    bool isSelectionEnabled() const { return m_selectionEnabled; }

    // 5250 terminal state (keyboard lock, insert mode, etc.)
    KeyboardState keyboardState() const { return m_keyboardState; }
    void setKeyboardState(KeyboardState state);
    bool insertMode() const { return m_insertMode; }
    void setInsertMode(bool enabled) { m_insertMode = enabled; update(); }
    bool messageWaiting() const { return m_messageWaiting; }
    void setMessageWaiting(bool on) { m_messageWaiting = on; emit terminalStateChanged(); }
    int icRow() const { return m_icRow; }
    int icCol() const { return m_icCol; }
    void setICAddress(int row, int col) { m_icRow = row; m_icCol = col; }
    int errorLineRow() const { return m_errorLineRow; }
    void setErrorLineRow(int row) { m_errorLineRow = row; }
    const uint8_t *cmdKeyMask() const { return m_cmdKeyMask; }
    void setCmdKeyMask(uint8_t m1, uint8_t m2, uint8_t m3) { m_cmdKeyMask[0]=m1; m_cmdKeyMask[1]=m2; m_cmdKeyMask[2]=m3; }
    void setSavedErrorLine(const QVector<ScreenCell> &line) { m_savedErrorLine = line; }

  public slots:
    void updateScreen();
    // Input processing (integrated)
    void processKeyEvent(QKeyEvent *event);
    void processEncodedInput(const QByteArray &data);
    void moveToNextField();
    void moveToPreviousField();
    void moveToFieldStart();
    void moveToFieldEnd();
    void moveCursor(int row, int col);
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorUp();
    void moveCursorDown();

  signals:
    void screenSizeChanged(int rows, int cols);
    void inputReady(const QByteArray &data);
    void terminalStateChanged();

  protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  private slots:
    void onScreenChanged();
    void onCursorMoved(const QPoint &pos);
    void onBlinkTimer();

  private:
    // Rendering
    void renderScreen(QPainter &painter);
    void renderCell(QPainter &painter, int row, int col, const ScreenCell &cell);
    void renderCursorRules(QPainter &painter);
    void renderSelectionBorder(QPainter &painter);
    QColor getColorForCode(uint8_t colorCode) const;
    void updateCursorWidget();

    // Layout
    void calculateCellSize();
    QPoint cellPosition(int row, int col) const;
    QRect cellRect(int row, int col) const;
    QPoint screenOffset() const;                        // Offset to center the screen
    QPoint screenToCell(const QPoint &screenPos) const; // Convert screen coordinates to cell coordinates
    bool isCellSelected(int row,
                        int col) const; // Check if a cell is in the selection
    bool hasSelection() const;          // Check if there is an active selection
    void copySelection();               // Copy selected text to clipboard
    void clearSelection();              // Clear the current selection
    // Input helpers
    ScreenBuffer::Field findNextField(int startRow, int startCol) const;
    ScreenBuffer::Field findPreviousField(int startRow, int startCol) const;
    bool isValidEditPosition(int row, int col) const;
    QByteArray buildAIDResponse(uint8_t aidByte);
    void handleBackspace();
    void handleDelete();
    void handleEraseInput();
    void rightAdjustField(int row, int col);

    ScreenBuffer *m_screenBuffer;
    core::KeyboardEncoder *m_encoder;
    QFont m_baseFont; // Base font set by user
    QFont m_font;     // Scaled font for rendering
    QSize m_cellSize;
    QColor m_bgColor;
    QColor m_fgColor;
    QVector<QColor> m_colorScheme;
    QPoint m_cursorPos;
    Q5250Cursor *m_cursorWidget = nullptr;

    // Cursor blinking
    QTimer *m_blinkTimer;
    int m_cursorBlinkRate;
    bool m_cursorBlinkState;
    bool m_blinkTextState = true;
    bool m_cursorEnabled = true;

    // 27×132 mode support
    bool m_extendedMode;

    // Selection state
    bool m_selecting;
    QPoint m_selectionStart; // Cell coordinates (row, col)
    QPoint m_selectionEnd;   // Cell coordinates (row, col)
    bool m_selectionEnabled = true;

    // Overlay
    bool m_showCursorRules;
    bool m_showFieldProtection;
    bool m_showInputFields;

    // 5250 terminal state
    KeyboardState m_keyboardState;
    bool m_insertMode;
    bool m_messageWaiting;
    int m_icRow;
    int m_icCol;
    int m_errorLineRow;                // Error line row from SOH (default: last row)
    uint8_t m_cmdKeyMask[3];           // SOH command key masks
    QVector<ScreenCell> m_savedErrorLine; // Saved error line contents for Error Reset
};

} // namespace ui::widgets
