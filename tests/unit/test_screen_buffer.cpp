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

#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QtTest/QtTest>

using namespace ui::widgets;

class TestScreenBuffer : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testInitialization();
    void testResize();
    void testCellAccess();
    void testCursorManagement();
    void testFieldManagement();
    void testClear();
    void testScroll();
    void testWriteOperations();
    void testAttributes();
    void testProtectedFields();

  private:
    ScreenBuffer *m_buffer;
};

void TestScreenBuffer::init() {
    m_buffer = new ScreenBuffer(24, 80, this);
}

void TestScreenBuffer::cleanup() {
    m_buffer->deleteLater();
    m_buffer = nullptr;
}

void TestScreenBuffer::testInitialization() {
    QCOMPARE(m_buffer->rows(), 24);
    QCOMPARE(m_buffer->cols(), 80);
    QCOMPARE(m_buffer->cursorPosition(), QPoint(0, 0));
    QVERIFY(m_buffer->isCursorVisible());
}

void TestScreenBuffer::testResize() {
    m_buffer->resize(27, 132);
    QCOMPARE(m_buffer->rows(), 27);
    QCOMPARE(m_buffer->cols(), 132);
}

void TestScreenBuffer::testCellAccess() {
    // Test character access
    uint8_t ch = m_buffer->character(0, 0);
    QCOMPARE(ch, static_cast<uint8_t>(0x40)); // EBCDIC space

    // Test writing
    m_buffer->writeChar(0, 0, 0xC1); // EBCDIC 'A'
    QCOMPARE(m_buffer->character(0, 0), static_cast<uint8_t>(0xC1));
}

void TestScreenBuffer::testCursorManagement() {
    m_buffer->setCursorPosition(10, 20);
    QCOMPARE(m_buffer->cursorPosition(), QPoint(20, 10));

    m_buffer->setCursorVisible(false);
    QVERIFY(!m_buffer->isCursorVisible());

    m_buffer->setCursorVisible(true);
    QVERIFY(m_buffer->isCursorVisible());
}

void TestScreenBuffer::testFieldManagement() {
    // Create a protected field
    m_buffer->setField(5, 10, 20, true);

    QVERIFY(m_buffer->isInField(5, 10));
    QVERIFY(m_buffer->isInField(5, 15));
    QVERIFY(m_buffer->isInField(5, 29));
    QVERIFY(!m_buffer->isInField(5, 30));
    QVERIFY(m_buffer->isProtected(5, 10));

    // Create an unprotected field
    m_buffer->setField(6, 5, 15, false);
    QVERIFY(m_buffer->isInField(6, 5));
    QVERIFY(!m_buffer->isProtected(6, 5));
}

void TestScreenBuffer::testClear() {
    // Write some data
    m_buffer->writeChar(0, 0, 0xC1);
    m_buffer->writeChar(0, 1, 0xC2);

    // Clear entire screen
    m_buffer->clear();

    QCOMPARE(m_buffer->character(0, 0), static_cast<uint8_t>(0x40));
    QCOMPARE(m_buffer->character(0, 1), static_cast<uint8_t>(0x40));

    // Clear a row
    m_buffer->writeChar(5, 0, 0xC1);
    m_buffer->clearRow(5);
    QCOMPARE(m_buffer->character(5, 0), static_cast<uint8_t>(0x40));
}

void TestScreenBuffer::testScroll() {
    // Write data to first row
    m_buffer->writeChar(0, 0, 0xC1);
    m_buffer->writeChar(0, 1, 0xC2);

    // Scroll up
    m_buffer->scrollUp(1);

    // First row should now be empty (or contain what was in row 1)
    // Original row 0 data should be gone

    // Write data to last row
    m_buffer->writeChar(23, 0, 0xC3);

    // Scroll down
    m_buffer->scrollDown(1);

    // Last row should now be empty
    QCOMPARE(m_buffer->character(23, 0), static_cast<uint8_t>(0x40));
}

void TestScreenBuffer::testWriteOperations() {
    // Test writeChar
    m_buffer->writeChar(0, 0, 0xC1);
    QCOMPARE(m_buffer->character(0, 0), static_cast<uint8_t>(0xC1));

    // Test writeString
    QByteArray data;
    data.append(static_cast<char>(0xC1));
    data.append(static_cast<char>(0xC2));
    data.append(static_cast<char>(0xC3));
    m_buffer->writeString(1, 0, data);

    QCOMPARE(m_buffer->character(1, 0), static_cast<uint8_t>(0xC1));
    QCOMPARE(m_buffer->character(1, 1), static_cast<uint8_t>(0xC2));
    QCOMPARE(m_buffer->character(1, 2), static_cast<uint8_t>(0xC3));

    // Test eraseWrite
    m_buffer->eraseWrite(1, 0, 3);
    QCOMPARE(m_buffer->character(1, 0), static_cast<uint8_t>(0x40));
    QCOMPARE(m_buffer->character(1, 1), static_cast<uint8_t>(0x40));
    QCOMPARE(m_buffer->character(1, 2), static_cast<uint8_t>(0x40));
}

void TestScreenBuffer::testAttributes() {
    CellAttributes attr;
    attr.color = 2; // Green
    attr.reverse = true;
    attr.blink = true;
    attr.underline = true;

    m_buffer->setAttributes(0, 0, attr);

    CellAttributes retrieved = m_buffer->attributes(0, 0);
    QCOMPARE(retrieved.color, static_cast<uint8_t>(2));
    QVERIFY(retrieved.reverse);
    QVERIFY(retrieved.blink);
    QVERIFY(retrieved.underline);

    // Test individual attribute setters
    m_buffer->setColor(0, 1, 5);
    QCOMPARE(m_buffer->attributes(0, 1).color, static_cast<uint8_t>(5));

    m_buffer->setReverse(0, 2, true);
    QVERIFY(m_buffer->attributes(0, 2).reverse);

    m_buffer->setBlink(0, 3, true);
    QVERIFY(m_buffer->attributes(0, 3).blink);

    m_buffer->setUnderline(0, 4, true);
    QVERIFY(m_buffer->attributes(0, 4).underline);
}

void TestScreenBuffer::testProtectedFields() {
    // Set a protected field
    m_buffer->setField(5, 10, 20, true);

    // Try to modify a protected field (should be protected)
    QVERIFY(m_buffer->isProtected(5, 15));

    // Set an unprotected field
    m_buffer->setField(6, 5, 15, false);
    QVERIFY(!m_buffer->isProtected(6, 10));
}

QTEST_MAIN(TestScreenBuffer)
#include "test_screen_buffer.moc"
