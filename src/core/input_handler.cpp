#include "input_handler.h"
#include <QDebug>
#include <QKeyEvent>

namespace core {

InputHandler::InputHandler(display::ScreenBuffer *screenBuffer, QObject *parent)
    : QObject(parent), m_screenBuffer(screenBuffer),
      m_encoder(new KeyboardEncoder(this)) {
  if (m_screenBuffer) {
    m_cursorPos = m_screenBuffer->cursorPosition();
  }

  connect(m_encoder, &KeyboardEncoder::keyEncoded, this,
          &InputHandler::processEncodedInput);
}

void InputHandler::processKeyEvent(QKeyEvent *event) {
  if (!m_screenBuffer) {
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

void InputHandler::processEncodedInput(const QByteArray &data) {
  if (data.isEmpty() || !m_screenBuffer) {
    return;
  }

  uint8_t firstByte = static_cast<uint8_t>(data[0]);

  // Check if it's an AID (Attention ID) - special key or PF key
  // AID codes are typically 0x7D (Enter), 0xF1-0xF9 (PF1-PF9), etc.
  bool isAID = (firstByte >= 0x6C && firstByte <= 0xFF) ||
               (firstByte >= 0xF1 && firstByte <= 0xF9) ||
               (firstByte >= 0x7A && firstByte <= 0x7D) ||
               (firstByte >= 0xC1 && firstByte <= 0xC9) ||
               (firstByte >= 0x4A && firstByte <= 0x4C);

  if (isAID) {
    // Special key or PF key - send to server
    emit inputReady(data);
    return;
  }

  // Normal character input
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  // Check if position is valid and not protected
  if (isValidEditPosition(row, col)) {
    insertCharacter(firstByte);
    // Move cursor right
    if (col + 1 < m_screenBuffer->cols()) {
      moveCursor(row, col + 1);
    }
    emit screenUpdateNeeded();
  }

  // Always send input to server
  emit inputReady(data);
}

void InputHandler::moveToNextField() {
  if (!m_screenBuffer) {
    return;
  }

  m_cursorPos = m_screenBuffer->cursorPosition();
  display::ScreenBuffer::Field nextField =
      findNextField(m_cursorPos.y(), m_cursorPos.x());

  if (nextField.length > 0) {
    moveCursor(nextField.startRow, nextField.startCol);
  }
}

void InputHandler::moveToPreviousField() {
  if (!m_screenBuffer) {
    return;
  }

  m_cursorPos = m_screenBuffer->cursorPosition();
  display::ScreenBuffer::Field prevField =
      findPreviousField(m_cursorPos.y(), m_cursorPos.x());

  if (prevField.length > 0) {
    moveCursor(prevField.startRow, prevField.startCol);
  }
}

void InputHandler::moveToFieldStart() {
  if (!m_screenBuffer) {
    return;
  }

  m_cursorPos = m_screenBuffer->cursorPosition();
  display::ScreenBuffer::Field field =
      m_screenBuffer->getField(m_cursorPos.y(), m_cursorPos.x());

  if (field.length > 0) {
    moveCursor(field.startRow, field.startCol);
  }
}

void InputHandler::moveToFieldEnd() {
  if (!m_screenBuffer) {
    return;
  }

  m_cursorPos = m_screenBuffer->cursorPosition();
  display::ScreenBuffer::Field field =
      m_screenBuffer->getField(m_cursorPos.y(), m_cursorPos.x());

  if (field.length > 0) {
    int endCol = field.startCol + field.length - 1;
    moveCursor(field.startRow, endCol);
  }
}

void InputHandler::moveCursor(int row, int col) {
  if (m_screenBuffer) {
    m_screenBuffer->setCursorPosition(row, col);
    m_cursorPos = m_screenBuffer->cursorPosition();
  }
}

void InputHandler::moveCursorLeft() {
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  if (col > 0) {
    moveCursor(row, col - 1);
  } else if (row > 0) {
    moveCursor(row - 1, m_screenBuffer->cols() - 1);
  }
}

void InputHandler::moveCursorRight() {
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  if (col + 1 < m_screenBuffer->cols()) {
    moveCursor(row, col + 1);
  } else if (row + 1 < m_screenBuffer->rows()) {
    moveCursor(row + 1, 0);
  }
}

void InputHandler::moveCursorUp() {
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  if (row > 0) {
    moveCursor(row - 1, col);
  }
}

void InputHandler::moveCursorDown() {
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  if (row + 1 < m_screenBuffer->rows()) {
    moveCursor(row + 1, col);
  }
}

void InputHandler::insertCharacter(uint8_t ebcdic) {
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  if (isValidEditPosition(row, col)) {
    display::CellAttributes attr = m_screenBuffer->attributes(row, col);
    m_screenBuffer->writeChar(row, col, ebcdic, attr);
  }
}

void InputHandler::deleteCharacter() {
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  if (!isValidEditPosition(row, col)) {
    return;
  }

  // Shift characters left
  for (int c = col; c < m_screenBuffer->cols() - 1; ++c) {
    uint8_t nextChar = m_screenBuffer->character(row, c + 1);
    display::CellAttributes attr = m_screenBuffer->attributes(row, c + 1);
    m_screenBuffer->writeChar(row, c, nextChar, attr);
  }

  // Clear last character
  m_screenBuffer->writeChar(row, m_screenBuffer->cols() - 1, 0x40,
                            display::CellAttributes());
}

void InputHandler::backspaceCharacter() {
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  if (col > 0 && isValidEditPosition(row, col - 1)) {
    moveCursorLeft();
    deleteCharacter();
  }
}

void InputHandler::clearField() {
  m_cursorPos = m_screenBuffer->cursorPosition();
  int row = m_cursorPos.y();
  int col = m_cursorPos.x();

  display::ScreenBuffer::Field field = m_screenBuffer->getField(row, col);
  if (field.length > 0 && !field.protected_field) {
    m_screenBuffer->clearField(row, col);
  }
}

display::ScreenBuffer::Field InputHandler::findNextField(int startRow,
                                                         int startCol) const {
  if (!m_screenBuffer) {
    return display::ScreenBuffer::Field();
  }

  // Search from current position forward
  for (int row = startRow; row < m_screenBuffer->rows(); ++row) {
    int startColSearch = (row == startRow) ? startCol + 1 : 0;
    for (int col = startColSearch; col < m_screenBuffer->cols(); ++col) {
      display::ScreenBuffer::Field field = m_screenBuffer->getField(row, col);
      if (field.length > 0 && !field.protected_field) {
        return field;
      }
    }
  }

  return display::ScreenBuffer::Field();
}

display::ScreenBuffer::Field
InputHandler::findPreviousField(int startRow, int startCol) const {
  if (!m_screenBuffer) {
    return display::ScreenBuffer::Field();
  }

  // Search from current position backward
  for (int row = startRow; row >= 0; --row) {
    int startColSearch =
        (row == startRow) ? startCol - 1 : m_screenBuffer->cols() - 1;
    for (int col = startColSearch; col >= 0; --col) {
      display::ScreenBuffer::Field field = m_screenBuffer->getField(row, col);
      if (field.length > 0 && !field.protected_field) {
        return field;
      }
    }
  }

  return display::ScreenBuffer::Field();
}

bool InputHandler::isValidEditPosition(int row, int col) const {
  if (!m_screenBuffer) {
    return false;
  }

  if (row < 0 || row >= m_screenBuffer->rows() || col < 0 ||
      col >= m_screenBuffer->cols()) {
    return false;
  }

  // Check if position is protected
  if (m_screenBuffer->isProtected(row, col)) {
    return false;
  }

  return true;
}

} // namespace core
