#include "core/input_handler.h"
#include "display/screen_buffer.h"
#include <QKeyEvent>
#include <QtTest/QtTest>

using namespace core;
using namespace display;

class TestInputHandler : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();

  void testInitialization();
  void testCursorMovement();
  void testFieldNavigation();
  void testCharacterInput();
  void testProtectedField();
  void testDeleteOperations();

private:
  InputHandler *m_handler;
  ScreenBuffer *m_screenBuffer;
};

void TestInputHandler::init() {
  m_screenBuffer = new ScreenBuffer(24, 80, this);
  m_handler = new InputHandler(m_screenBuffer, this);
}

void TestInputHandler::cleanup() {
  m_handler->deleteLater();
  m_screenBuffer->deleteLater();
  m_handler = nullptr;
  m_screenBuffer = nullptr;
}

void TestInputHandler::testInitialization() {
  QVERIFY(m_handler != nullptr);
  QVERIFY(m_screenBuffer != nullptr);
}

void TestInputHandler::testCursorMovement() {
  // Test moving cursor
  m_handler->moveCursor(5, 10);
  QCOMPARE(m_screenBuffer->cursorPosition(), QPoint(10, 5));

  // Test cursor movement functions
  m_handler->moveCursorRight();
  QCOMPARE(m_screenBuffer->cursorPosition(), QPoint(11, 5));

  m_handler->moveCursorLeft();
  QCOMPARE(m_screenBuffer->cursorPosition(), QPoint(10, 5));

  m_handler->moveCursorDown();
  QCOMPARE(m_screenBuffer->cursorPosition(), QPoint(10, 6));

  m_handler->moveCursorUp();
  QCOMPARE(m_screenBuffer->cursorPosition(), QPoint(10, 5));
}

void TestInputHandler::testFieldNavigation() {
  // Create some fields
  m_screenBuffer->setField(5, 10, 20, false); // Unprotected field
  m_screenBuffer->setField(6, 5, 15, false);  // Another unprotected field

  // Move to first field
  m_handler->moveCursor(5, 10);

  // Test field navigation
  m_handler->moveToNextField();
  // Should move to field at row 6, col 5
  QPoint pos = m_screenBuffer->cursorPosition();
  QVERIFY(pos.y() == 6);

  m_handler->moveToPreviousField();
  // Should move back to field at row 5, col 10
  pos = m_screenBuffer->cursorPosition();
  QVERIFY(pos.y() == 5);
}

void TestInputHandler::testCharacterInput() {
  // Create an unprotected field
  m_screenBuffer->setField(5, 10, 20, false);
  m_handler->moveCursor(5, 10);

  // Insert a character
  m_handler->insertCharacter(0xC1); // EBCDIC 'A'

  QCOMPARE(m_screenBuffer->character(5, 10), static_cast<uint8_t>(0xC1));
}

void TestInputHandler::testProtectedField() {
  // Create a protected field
  m_screenBuffer->setField(5, 10, 20, true);
  m_handler->moveCursor(5, 10);

  // Try to insert character in protected field
  uint8_t before = m_screenBuffer->character(5, 10);
  m_handler->insertCharacter(0xC1);

  // Character should not change (protected)
  QCOMPARE(m_screenBuffer->character(5, 10), before);
}

void TestInputHandler::testDeleteOperations() {
  // Create an unprotected field and add some data
  m_screenBuffer->setField(5, 10, 20, false);
  m_handler->moveCursor(5, 10);

  m_screenBuffer->writeChar(5, 10, 0xC1); // 'A'
  m_screenBuffer->writeChar(5, 11, 0xC2); // 'B'
  m_screenBuffer->writeChar(5, 12, 0xC3); // 'C'

  m_handler->moveCursor(5, 11);

  // Test delete
  m_handler->deleteCharacter();
  // 'B' should be deleted, 'C' should shift left
  QCOMPARE(m_screenBuffer->character(5, 11), static_cast<uint8_t>(0xC3));

  // Test backspace
  m_handler->moveCursor(5, 12);
  m_handler->backspaceCharacter();
  // Should delete character at position 11 and move cursor left
}

QTEST_MAIN(TestInputHandler)
#include "test_input_handler.moc"
