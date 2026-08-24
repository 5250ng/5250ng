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

#pragma once

#include "core/ebcdic.h"
#include "core/hotspot_detector.h"
#include "core/match_replace_engine.h"
#include "core/keyboard_encoder.h"
#include "core/keyboard_mapping.h"
#include "core/screen_history.h"
#include "screen_buffer.h"
#include "ui/themes/terminal_theme.h"
#include <QClipboard>
#include <QImage>
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

    // Cell grid overlay
    void setShowCellGrid(bool enabled);
    void toggleCellGrid();
    bool showCellGrid() const { return m_showCellGrid; }

    // Hotspots
    void setHotspotsEnabled(bool enabled);
    void toggleHotspots();
    bool hotspotsEnabled() const { return m_hotspotDetector.isEnabled(); }
    const QVector<core::Hotspot> &hotspots() const { return m_hotspots; }

    // Match and Replace
    void setMatchReplaceEngine(core::MatchReplaceEngine *engine);
    core::MatchReplaceEngine *matchReplaceEngine() const { return m_matchReplace; }
    void refreshMatchReplaceOverlay();

    // Screen history
    core::ScreenHistory *screenHistory() { return &m_screenHistory; }
    bool isViewingHistory() const { return m_historyIndex >= 0; }
    int historyIndex() const { return m_historyIndex; }
    void viewHistoryScreen(int index);
    void exitHistoryView();

    // Screen buffer access
    ScreenBuffer *screenBuffer() { return m_screenBuffer; }
    const ScreenBuffer *screenBuffer() const { return m_screenBuffer; }

    // IBM 5292 Model 2 graphics are retained independently from the
    // alphanumeric presentation space and composited during painting.
    void setGddmGraphicsPlane(const QImage &plane, bool visible);
    const QImage &gddmGraphicsPlane() const { return m_gddmGraphicsPlane; }
    bool gddmGraphicsVisible() const { return m_gddmGraphicsVisible; }

    // Display configuration
    void setScreenSize(int rows, int cols);
    int screenRows() const { return m_screenBuffer->rows(); }
    int screenCols() const { return m_screenBuffer->cols(); }

    // Font configuration
    void setFont(const QFont &font);
    QFont font() const { return m_font; }

    // Terminal theme
    void applyTerminalTheme(const ui::themes::TerminalTheme &theme);

    // Grid layout mode
    void setGridMode(ui::themes::TerminalTheme::GridMode mode);
    ui::themes::TerminalTheme::GridMode gridMode() const { return m_gridMode; }

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

    // Read-only mode (blocks user keyboard/mouse input except copy)
    // MCP-injected input bypasses read-only when m_mcpInjecting is set.
    void setReadOnly(bool readOnly) { m_readOnly = readOnly; }
    bool isReadOnly() const { return m_readOnly; }

    // MCP injection guard — set by AgentScriptRunner around sendEvent() calls
    void setMcpInjecting(bool injecting) { m_mcpInjecting = injecting; }
    bool isMcpInjecting() const { return m_mcpInjecting; }

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
    void processEncodedInput(const QByteArray &data, bool isAID = false);
    void moveToNextField();
    void moveToPreviousField();
    void moveToFieldStart();
    void moveToFieldEnd();
    void moveCursor(int row, int col);
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorUp();
    void moveCursorDown();

    // Execute a logical 5250 action resolved either from a host key chord
    // (via KeyboardMapping) or from a click on the on-screen virtual keyboard.
    void dispatchMappedAction(core::MappedAction action);

    // Inject a single character as if typed into the active field (used by
    // the virtual keyboard widget). Respects read-only/keyboard-lock state.
    void dispatchCharacter(QChar ch);

    void setReadType(uint8_t readType) { m_readType = readType; }
    uint8_t readType() const { return m_readType; }

  signals:
    void screenSizeChanged(int rows, int cols);
    void inputReady(const QByteArray &data);
    void attentionRequested();
    void systemRequestRequested();
    void terminalStateChanged();
    void hotspotActivated(const core::Hotspot &hotspot);
    void historyViewChanged(int index, int total);
    void cellSizeChanged();
    void keyRecorded(int key, Qt::KeyboardModifiers mods, const QString &text);
    void aidKeyRecorded(uint8_t aidByte);

  protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
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
    void renderCellGrid(QPainter &painter);
    void renderSelectionBorder(QPainter &painter);
    // Background image rendering moved to QBackgroundImageWidget at tab level
    QColor getColorForCode(uint8_t colorCode) const;
    void updateCursorWidget();

    // Layout
    void calculateCellSize();
  public:
    qreal cellWidthF() const { return m_cellWidthF; }
    qreal cellHeightF() const { return m_cellHeightF; }
    void overrideCellWidth(qreal w);
    void overrideCellHeight(qreal h);
    QFont scaledFont() const { return m_font; }
    QFont baseFont() const { return m_baseFont; }
  private:
    QPoint cellPosition(int row, int col) const;
    QRect cellRect(int row, int col) const;
    QPoint screenOffset() const;                        // Offset to center the screen
    QPoint screenToCell(const QPoint &screenPos) const; // Convert screen coordinates to cell coordinates
    bool isCellSelected(int row,
                        int col) const; // Check if a cell is in the selection
    bool hasSelection() const;
  public:
    void copySelection();
    void clearSelection();
    void selectAll();
  private:
    // Input helpers
    ScreenBuffer::Field findNextField(int startRow, int startCol) const;
    ScreenBuffer::Field findPreviousField(int startRow, int startCol) const;
    bool isValidEditPosition(int row, int col) const;
    QByteArray buildAIDResponse(uint8_t aidByte);
    void handleBackspace();
    void handleDelete();
    void handleEraseInput();
    void handleFieldExit();
    void handleEraseEOF();
    void handleEraseField();
    void handleDup();
    void handleFieldPlus();
    void handleFieldMinus();
  public:
    void handlePaste();
  private:
    void rightAdjustField(int row, int col);

    ScreenBuffer *m_screenBuffer;
    core::KeyboardEncoder *m_encoder;
    QFont m_baseFont; // Base font set by user
    QFont m_font;     // Scaled font for rendering
    QSize m_cellSize;
    qreal m_cellWidthF = 8.0;  // Precise cell width for sub-pixel positioning
    qreal m_cellHeightF = 16.0; // Precise cell height for sub-pixel positioning
    QColor m_bgColor;
    QColor m_fgColor;
    QVector<QColor> m_colorScheme;
    QPoint m_cursorPos;
    Q5250Cursor *m_cursorWidget = nullptr;

    // Background opacity (< 1 when background image is active at tab level)
    double m_bgOpacity = 1.0;

    // Selection & indicator colors (from theme)
    QColor m_selectionBgColor = QColor(255, 255, 0, 64);
    QColor m_selectionFgColor = QColor(255, 255, 255);
    QColor m_selectionBorderColor = QColor(255, 255, 0);
    QColor m_fieldIndicatorColor = QColor(0, 128, 255, 64);

    // Column separator (from theme)
    bool m_colSepEnabled = true;
    QColor m_colSepColor = QColor(128, 128, 128);
    ui::themes::TerminalTheme::ColSepStyle m_colSepStyle =
        ui::themes::TerminalTheme::Solid;

    // Cursor blinking
    QTimer *m_blinkTimer;
    int m_cursorBlinkRate;
    bool m_cursorBlinkState;
    bool m_blinkTextState = true;
    bool m_cursorEnabled = true;

    // Grid layout mode
    ui::themes::TerminalTheme::GridMode m_gridMode = ui::themes::TerminalTheme::Packed;

    // 27×132 mode support
    bool m_extendedMode;

    // Selection state
    bool m_selecting;
    QPoint m_selectionStart; // Cell coordinates (row, col)
    QPoint m_selectionEnd;   // Cell coordinates (row, col)
    bool m_selectionEnabled = true;
    bool m_readOnly = false;
    bool m_mcpInjecting = false;

    // Overlay
    bool m_showCursorRules;
    bool m_showFieldProtection;
    bool m_showInputFields;
    bool m_showCellGrid = false;
    QColor m_cellGridColor = QColor(255, 255, 255, 40);
    QImage m_gddmGraphicsPlane;
    bool m_gddmGraphicsVisible = false;

    // 5250 terminal state
    KeyboardState m_keyboardState;
    bool m_insertMode;
    bool m_messageWaiting;
    int m_icRow;
    int m_icCol;
    int m_errorLineRow;                // Error line row from SOH (default: last row)
    uint8_t m_cmdKeyMask[3];           // SOH command key masks
    QVector<ScreenCell> m_savedErrorLine; // Saved error line contents for Error Reset
    uint8_t m_readType = 0x52; // 0x52=READ_MDT (default), 0x42=READ_INPUT

    // Hotspot detection
    core::HotspotDetector m_hotspotDetector;
    QVector<core::Hotspot> m_hotspots;
    void refreshHotspots();
    void renderHotspots(QPainter &painter);

    // Match and Replace overlay
    core::MatchReplaceEngine *m_matchReplace = nullptr;

    // Screen history / scrollback
    core::ScreenHistory m_screenHistory;
    int m_historyIndex = -1; // -1 = live screen
    void pushScreenToHistory();
};

} // namespace ui::widgets
