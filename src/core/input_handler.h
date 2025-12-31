#pragma once

#include "../display/screen_buffer.h"
#include "keyboard_encoder.h"
#include <QObject>
#include <QPoint>

namespace core {

// Input handler for TN5250 terminal
// Manages keyboard input, field navigation, and cursor movement
class InputHandler : public QObject {
  Q_OBJECT

public:
  explicit InputHandler(display::ScreenBuffer *screenBuffer,
                        QObject *parent = nullptr);

  // Process a key event
  void processKeyEvent(QKeyEvent *event);

  // Process encoded keyboard data (from encoder)
  void processEncodedInput(const QByteArray &data);

  // Field navigation
  void moveToNextField();
  void moveToPreviousField();
  void moveToFieldStart();
  void moveToFieldEnd();

  // Cursor movement
  void moveCursor(int row, int col);
  void moveCursorLeft();
  void moveCursorRight();
  void moveCursorUp();
  void moveCursorDown();

  // Field editing
  void insertCharacter(uint8_t ebcdic);
  void deleteCharacter();
  void backspaceCharacter();
  void clearField();

signals:
  // Emitted when input should be sent to server
  void inputReady(const QByteArray &data);

  // Emitted when screen needs update
  void screenUpdateNeeded();

private:
  // Field navigation helpers
  display::ScreenBuffer::Field findNextField(int startRow, int startCol) const;
  display::ScreenBuffer::Field findPreviousField(int startRow,
                                                 int startCol) const;
  bool isValidEditPosition(int row, int col) const;

  display::ScreenBuffer *m_screenBuffer;
  KeyboardEncoder *m_encoder;
  QPoint m_cursorPos;
};

} // namespace core
