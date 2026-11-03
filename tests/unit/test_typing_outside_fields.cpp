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

#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// Regression test for issue #373: a character typed with the cursor on
// host text outside every input field overwrote that text.
class TestTypingOutsideFields : public QObject {
    Q_OBJECT

  private slots:
    void testCharacterOutsideFieldsIsRefused() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setKeyboardState(KeyboardState::Unlocked);
        auto *buf = widget.screenBuffer();
        // Menu text "6." at row 9 and one input field on the command line
        buf->writeChar(8, 6, 0xF6);
        buf->writeChar(8, 7, 0x4B);
        buf->setField(19, 6, 20, false);
        widget.moveCursor(8, 6);
        widget.dispatchCharacter('Z');
        widget.dispatchCharacter('Z');
        QCOMPARE(int(buf->character(8, 6)), 0xF6);
        QCOMPARE(int(buf->character(8, 7)), 0x4B);

        // Typing in the input field still works
        widget.moveCursor(19, 6);
        widget.dispatchCharacter('A');
        QCOMPARE(int(buf->character(19, 6)), 0xC1);
    }
};

QTEST_MAIN(TestTypingOutsideFields)
#include "test_typing_outside_fields.moc"
