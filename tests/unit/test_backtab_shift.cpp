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
#include <QtTest/QtTest>

using core::KeyboardMapping;
using core::MappedAction;
using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #275: Qt on xcb and Windows delivers Shift+Tab
// as Key_Backtab with Shift still set, which matched no default binding.
class TestBacktabShift : public QObject {
    Q_OBJECT

  private slots:
    void init() { KeyboardMapping::instance().resetToDefaults(); }

    void testBacktabWithShiftIsBackTab() {
        QCOMPARE(KeyboardMapping::instance().lookup(Qt::Key_Backtab, Qt::ShiftModifier),
                 MappedAction::BackTab);
    }

    void testExplicitBindingWins() {
        auto &mapping = KeyboardMapping::instance();
        mapping.setBinding({Qt::Key_Backtab, Qt::ShiftModifier}, MappedAction::FieldExit);
        QCOMPARE(mapping.lookup(Qt::Key_Backtab, Qt::ShiftModifier), MappedAction::FieldExit);
        QCOMPARE(mapping.lookup(Qt::Key_Backtab, Qt::NoModifier), MappedAction::BackTab);
    }

    void testShiftTabMovesToPreviousField() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setKeyboardState(KeyboardState::Unlocked);
        auto *buf = widget.screenBuffer();
        buf->setField(3, 10, 5, false);
        buf->setField(5, 10, 5, false);
        widget.moveCursor(5, 10);
        QTest::keyClick(&widget, Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(buf->cursorPosition(), QPoint(10, 3));
    }
};

QTEST_MAIN(TestBacktabShift)
#include "test_backtab_shift.moc"
