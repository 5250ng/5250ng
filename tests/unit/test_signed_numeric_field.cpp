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

#include "core/keyboard_mapping.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;
using ui::widgets::ScreenBuffer;

// Issue #240. A signed numeric field (FFW shift type 7, DDS type S) keeps
// its last position for the sign: blank for positive, '-' for negative.
// Field Exit, Field+ and Field- right-adjust the digits into the other
// positions, and the sign position is never sent: a '-' there sets the D
// zone of the last digit (5494 Functions Reference SC30-3533, FFW "Signed
// numeric" and READ INPUT FIELDS). Without this the host gets one byte too
// many and the program fails with CPF5257.
class TestSignedNumericField : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        m_widget = new Q5250ScreenWidget();
        m_widget->setScreenSize(24, 80);
        auto *screen = m_widget->screenBuffer();
        // FNUM 7S 2: 7 digits and the sign position
        screen->setField(10, 19, 8, false);
        screen->setFieldFFW(10, 19, 0x47, 0x00);
        // FRZ 8S 0 CHECK(RZ): right adjust, zero fill
        screen->setField(9, 19, 9, false);
        screen->setFieldFFW(9, 19, 0x47, 0x05);
        // A numeric-only field (shift type 3, right adjust blank fill) has
        // no sign position
        screen->setField(11, 19, 5, false);
        screen->setFieldFFW(11, 19, 0x43, 0x06);
        m_widget->setKeyboardState(KeyboardState::Unlocked);
    }
    void cleanup() { delete m_widget; }

    void fieldExitRightAdjustsAndKeepsSignPosition() {
        type(10, 19, "123");
        m_widget->dispatchMappedAction(core::MappedAction::FieldExit);
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("40404040f1f2f340"));
        QCOMPARE(sentField(10, 19).toHex(), QByteArray("40404040f1f2f3"));
    }

    void fieldPlusWritesBlankSign() {
        type(10, 19, "45");
        m_widget->dispatchMappedAction(core::MappedAction::FieldPlus);
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("4040404040f4f540"));
        QCOMPARE(sentField(10, 19).toHex(), QByteArray("4040404040f4f5"));
    }

    void fieldMinusWritesMinusAndSendsDZone() {
        type(10, 19, "123");
        m_widget->dispatchMappedAction(core::MappedAction::FieldMinus);
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("40404040f1f2f360"));
        // The sign position is not sent; the last digit carries the sign
        QCOMPARE(sentField(10, 19).toHex(), QByteArray("40404040f1f2d3"));
    }

    void zeroFillStaysOutOfSignPosition() {
        type(9, 19, "42");
        m_widget->dispatchMappedAction(core::MappedAction::FieldExit);
        QCOMPARE(fieldBytes(9, 19).toHex(), QByteArray("f0f0f0f0f0f0f4f240"));
        QCOMPARE(sentField(9, 19).toHex(), QByteArray("f0f0f0f0f0f0f4f2"));
    }

    // Field- then Field+ turns the sign back to positive
    void fieldPlusAfterFieldMinusClearsSign() {
        type(10, 19, "7");
        m_widget->dispatchMappedAction(core::MappedAction::FieldMinus);
        m_widget->screenBuffer()->setCursorPosition(10, 26);
        m_widget->dispatchMappedAction(core::MappedAction::FieldPlus);
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("404040404040f740"));
    }

    // Numeric-only fields keep their own rule: Field- sets the D zone of
    // the last digit, and every position is sent
    void numericOnlyFieldUnchanged() {
        type(11, 19, "123");
        m_widget->dispatchMappedAction(core::MappedAction::FieldMinus);
        QCOMPARE(fieldBytes(11, 19).toHex(), QByteArray("4040f1f2d3"));
        QCOMPARE(sentField(11, 19).toHex(), QByteArray("4040f1f2d3"));
    }

    // #459: the sign position takes no keystroke, and a full field
    // advances like any full field. The 8th digit is refused
    void typingStopsBeforeSignPosition() {
        type(10, 19, "1234567");
        // Seven digits fill the field: the cursor moved to the next field
        QCOMPARE(m_widget->screenBuffer()->cursorPosition(), QPoint(19, 11));
        m_widget->screenBuffer()->setCursorPosition(10, 26); // the sign position
        m_widget->dispatchCharacter('8');
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("f1f2f3f4f5f6f740"));
        QCOMPARE(sentField(10, 19).toHex(), QByteArray("f1f2f3f4f5f6f7"));
    }

    void signsAreNotTypedIntoDigits() {
        type(10, 19, "1-2+");
        QCOMPARE(fieldBytes(10, 19).left(2).toHex(), QByteArray("f1f2"));
    }

    // After Field Exit the last digit position is full, so Insert is
    // refused; it used to shift the 3 into the sign position, which is not
    // sent, and the host got 9 then blanks then 12
    void insertDoesNotPushDigitIntoSignPosition() {
        type(10, 19, "123");
        m_widget->dispatchMappedAction(core::MappedAction::FieldExit);
        m_widget->screenBuffer()->setCursorPosition(10, 19);
        m_widget->dispatchMappedAction(core::MappedAction::Insert);
        m_widget->dispatchCharacter('9');
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("40404040f1f2f340"));
        QCOMPARE(sentField(10, 19).toHex(), QByteArray("40404040f1f2f3"));
    }

    // With room before the sign, Insert shifts the digits up to it
    void insertShiftsWithinDigits() {
        type(10, 19, "12");
        m_widget->screenBuffer()->setCursorPosition(10, 19);
        m_widget->dispatchMappedAction(core::MappedAction::Insert);
        m_widget->dispatchCharacter('9');
        QCOMPARE(fieldBytes(10, 19).left(3).toHex(), QByteArray("f9f1f2"));
    }

    // Insert into a full field is refused instead of losing the last digit
    void insertIntoFullFieldIsRefused() {
        type(10, 19, "1234567");
        m_widget->screenBuffer()->setCursorPosition(10, 19);
        m_widget->dispatchMappedAction(core::MappedAction::Insert);
        m_widget->dispatchCharacter('9');
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("f1f2f3f4f5f6f740"));
    }

    // Delete shifts the digits left; the '-' stays in the sign position
    void deleteKeepsSign() {
        type(10, 19, "123");
        m_widget->dispatchMappedAction(core::MappedAction::FieldMinus);
        m_widget->screenBuffer()->setCursorPosition(10, 19);
        m_widget->dispatchMappedAction(core::MappedAction::Delete);
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("404040f1f2f34060"));
        // Delete on the sign itself does nothing
        m_widget->screenBuffer()->setCursorPosition(10, 26);
        m_widget->dispatchMappedAction(core::MappedAction::Delete);
        QCOMPARE(fieldBytes(10, 19).toHex(), QByteArray("404040f1f2f34060"));
    }

    // The shortest signed field: one digit and the sign
    void twoPositionField() {
        auto *screen = m_widget->screenBuffer();
        screen->setField(12, 19, 2, false);
        screen->setFieldFFW(12, 19, 0x47, 0x00);
        type(12, 19, "5");
        m_widget->screenBuffer()->setCursorPosition(12, 20); // the sign position
        m_widget->dispatchMappedAction(core::MappedAction::FieldMinus);
        QCOMPARE(fieldBytes(12, 19).toHex(), QByteArray("f560"));
        QCOMPARE(sentField(12, 19).toHex(), QByteArray("d5"));
    }

    // FFW2 110: right adjust, blank fill
    void blankFillStaysOutOfSignPosition() {
        auto *screen = m_widget->screenBuffer();
        screen->setField(13, 19, 5, false);
        screen->setFieldFFW(13, 19, 0x47, 0x06);
        type(13, 19, "7");
        m_widget->dispatchMappedAction(core::MappedAction::FieldPlus);
        QCOMPARE(fieldBytes(13, 19).toHex(), QByteArray("404040f740"));
    }

    // Mandatory fill on a signed field: the digits must be full, the sign
    // position does not count
    void mandatoryFillIgnoresSignPosition() {
        auto *screen = m_widget->screenBuffer();
        screen->setField(14, 19, 4, false);
        screen->setFieldFFW(14, 19, 0x47, 0x07);
        screen->setField(15, 19, 4, false);
        screen->setFieldFFW(15, 19, 0x40, 0x00);
        type(14, 19, "12");
        m_widget->dispatchMappedAction(core::MappedAction::FieldExit);
        QCOMPARE(screen->cursorPosition(), QPoint(21, 14)); // refused: not full
        screen->setCursorPosition(14, 21);
        m_widget->dispatchCharacter('3');
        screen->setCursorPosition(14, 22); // Field Exit clears from the cursor
        m_widget->dispatchMappedAction(core::MappedAction::FieldExit);
        QCOMPARE(screen->cursorPosition(), QPoint(19, 15)); // accepted: next field
    }

  private:
    void type(int row, int col, const QString &text) {
        m_widget->screenBuffer()->setCursorPosition(row, col);
        for (QChar ch : text)
            m_widget->dispatchCharacter(ch);
    }

    QByteArray fieldBytes(int row, int col) const {
        auto *screen = m_widget->screenBuffer();
        return screen->getFieldData(screen->getField(row, col));
    }

    // The field's data in the Enter response (SBA row col data)
    QByteArray sentField(int row, int col) {
        QSignalSpy spy(m_widget, &Q5250ScreenWidget::inputReady);
        m_widget->setKeyboardState(KeyboardState::Unlocked);
        m_widget->dispatchMappedAction(core::MappedAction::Enter);
        if (spy.count() != 1) return "no response";
        const QByteArray sent = spy.at(0).at(0).toByteArray();
        const QByteArray sba = QByteArray("\x11") + char(row + 1) + char(col + 1);
        const int at = sent.indexOf(sba);
        if (at < 0) return "field not sent";
        const int next = sent.indexOf('\x11', at + 3);
        return sent.mid(at + 3, next < 0 ? -1 : next - at - 3);
    }

    Q5250ScreenWidget *m_widget = nullptr;
};

QTEST_MAIN(TestSignedNumericField)
#include "test_signed_numeric_field.moc"
