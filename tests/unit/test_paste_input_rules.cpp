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

#include "core/ebcdic.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QSignalSpy>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #281: paste wrote the clipboard straight into
// the screen buffer, ignoring read-only mode, the keyboard lock and the
// field rules that typed input goes through.
class TestPasteInputRules : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testRefusedWhileLocked();
    void testRefusedWhileErrorLocked();
    void testRefusedInReadOnlyMode();
    void testMonocaseFieldUppercases();
    void testNumericOnlyFieldRejectsLetters();
    void testTabMovesToNextField();
    void testStopsAfterAutoEnter();
    void testNoFieldMoveAfterAutoEnter();
    void testStartsInNextFieldFromProtectedPosition();

  private:
    // Input fields at row 3 col 10 and row 5 col 10, 5 characters each,
    // with the cursor at the start of the first one.
    void setUpFields(uint8_t ffw1, uint8_t ffw2);
    QString fieldText(int row) const;
    void paste(const QString &text);

    Q5250ScreenWidget *m_widget = nullptr;
};

void TestPasteInputRules::init() {
    m_widget = new Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
    m_widget->setKeyboardState(KeyboardState::Unlocked);
}

void TestPasteInputRules::cleanup() {
    delete m_widget;
    m_widget = nullptr;
}

void TestPasteInputRules::setUpFields(uint8_t ffw1, uint8_t ffw2) {
    auto *buf = m_widget->screenBuffer();
    for (int row : {3, 5}) {
        buf->setField(row, 10, 5, false);
        buf->setFieldFFW(row, 10, ffw1, ffw2);
    }
    m_widget->moveCursor(3, 10);
}

QString TestPasteInputRules::fieldText(int row) const {
    QString text;
    for (int c = 10; c < 15; ++c) {
        uint8_t ch = m_widget->screenBuffer()->character(row, c);
        text += (ch == 0x00 || ch == 0x40) ? QChar('.') : core::EBCDIC::ebcdicToChar(ch);
    }
    return text;
}

void TestPasteInputRules::paste(const QString &text) {
    QApplication::clipboard()->setText(text);
    m_widget->handlePaste();
}

void TestPasteInputRules::testRefusedWhileLocked() {
    setUpFields(0x40, 0x00);
    m_widget->setKeyboardState(KeyboardState::Locked);
    paste("abc");
    QCOMPARE(fieldText(3), QStringLiteral("....."));
}

void TestPasteInputRules::testRefusedWhileErrorLocked() {
    setUpFields(0x40, 0x00);
    m_widget->setKeyboardState(KeyboardState::ErrorLocked);
    paste("abc");
    QCOMPARE(fieldText(3), QStringLiteral("....."));
}

void TestPasteInputRules::testRefusedInReadOnlyMode() {
    setUpFields(0x40, 0x00);
    m_widget->setReadOnly(true);
    paste("abc");
    QCOMPARE(fieldText(3), QStringLiteral("....."));
}

void TestPasteInputRules::testMonocaseFieldUppercases() {
    setUpFields(0x40, 0x20);
    paste("abc");
    QCOMPARE(fieldText(3), QStringLiteral("ABC.."));
}

void TestPasteInputRules::testNumericOnlyFieldRejectsLetters() {
    setUpFields(0x43, 0x00);
    paste("1a2");
    QCOMPARE(fieldText(3), QStringLiteral("12..."));
}

void TestPasteInputRules::testTabMovesToNextField() {
    setUpFields(0x40, 0x00);
    paste("ab\tcd");
    QCOMPARE(fieldText(3), QStringLiteral("ab..."));
    QCOMPARE(fieldText(5), QStringLiteral("cd..."));
}

void TestPasteInputRules::testStopsAfterAutoEnter() {
    // Filling the first (auto-enter) field sends the screen and locks the
    // keyboard; the rest of the clipboard must not reach the next field.
    setUpFields(0x40, 0x80);
    paste("abcdefgh");
    QCOMPARE(fieldText(3), QStringLiteral("abcde"));
    QCOMPARE(m_widget->keyboardState(), KeyboardState::Locked);
    QCOMPARE(fieldText(5), QStringLiteral("....."));
}

void TestPasteInputRules::testNoFieldMoveAfterAutoEnter() {
    // A Tab after the character that auto-entered must not move the cursor
    // while the host owns the screen, and only one AID goes to the host.
    setUpFields(0x40, 0x80);
    QSignalSpy sent(m_widget, &Q5250ScreenWidget::inputReady);
    paste("abcde\tfg");
    QCOMPARE(sent.count(), 1);
    const QPoint cursor = m_widget->screenBuffer()->cursorPosition();
    QCOMPARE(cursor.y(), 3);
    QCOMPARE(fieldText(5), QStringLiteral("....."));
}

void TestPasteInputRules::testStartsInNextFieldFromProtectedPosition() {
    // Cursor in a protected (output) field before the input fields
    setUpFields(0x40, 0x00);
    m_widget->screenBuffer()->setField(1, 10, 5, true);
    m_widget->moveCursor(1, 10);
    paste("abc");
    QCOMPARE(fieldText(3), QStringLiteral("abc.."));
}

QTEST_MAIN(TestPasteInputRules)
#include "test_paste_input_rules.moc"
