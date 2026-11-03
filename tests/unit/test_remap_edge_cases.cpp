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
#include "ui/dialogs/keyboard_remap_dialog.h"
#include "ui/widgets/QVirtualKeyboardWidget/QVirtualKeyboardWidget.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>

using core::KeyboardMapping;
using core::MappedAction;
using ui::dialogs::KeyChordCaptureEdit;

// Regression tests for issue #299: Tab could not be captured in Remap
// Keyboard, and the virtual keyboard's Ctrl/Alt never reached a key.
class TestRemapEdgeCases : public QObject {
    Q_OBJECT

  private slots:
    void init() { KeyboardMapping::instance().resetToDefaults(); }

    void testCaptureEditTakesTab() {
        QWidget window;
        auto *layout = new QHBoxLayout(&window);
        auto *edit = new KeyChordCaptureEdit(&window);
        auto *other = new QLineEdit(&window);
        layout->addWidget(edit);
        layout->addWidget(other);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // Focus needs an active window, which a platform without a window
        // manager (xvfb) may not give; the capture itself does not
        window.activateWindow();
        const bool active = QTest::qWaitForWindowActive(&window, 2000);
        edit->setFocus();
        QSignalSpy captured(edit, &KeyChordCaptureEdit::chordCaptured);
        QTest::keyClick(edit, Qt::Key_Tab);
        QCOMPARE(captured.count(), 1);
        QCOMPARE(edit->chord().key, int(Qt::Key_Tab));
        if (active)
            QVERIFY(edit->hasFocus());
        QTest::keyClick(edit, Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(captured.count(), 2);
        QCOMPARE(edit->chord().key, int(Qt::Key_Backtab));
        if (active)
            QVERIFY(edit->hasFocus());
    }

    void testActionKeyReleasesLatch() {
        ui::widgets::QVirtualKeyboardWidget keyboard;
        QPushButton *ctrl = findButton(keyboard, QStringLiteral("Ctrl"));
        QPushButton *pf3 = findButton(keyboard, QStringLiteral("PF3"));
        QPushButton *a = findButton(keyboard, QStringLiteral("a"));
        QVERIFY(ctrl && pf3 && a);
        QSignalSpy chars(&keyboard, &ui::widgets::QVirtualKeyboardWidget::characterTriggered);
        ctrl->click();
        pf3->click();
        QVERIFY(!ctrl->isChecked());
        a->click();
        QCOMPARE(chars.count(), 1);
    }

    void testVirtualAltLatchesIntoNextKey() {
        // Alt then "-" is the Field- chord (Alt+Minus) on the physical keyboard
        ui::widgets::QVirtualKeyboardWidget keyboard;
        QPushButton *alt = findButton(keyboard, QStringLiteral("Alt"));
        QPushButton *minus = findButton(keyboard, QStringLiteral("-"));
        QVERIFY(alt && minus);
        QSignalSpy actions(&keyboard, &ui::widgets::QVirtualKeyboardWidget::actionTriggered);
        QSignalSpy chars(&keyboard, &ui::widgets::QVirtualKeyboardWidget::characterTriggered);
        alt->click();
        minus->click();
        QCOMPARE(chars.count(), 0);
        QCOMPARE(actions.count(), 1);
        QCOMPARE(actions.at(0).at(0).value<MappedAction>(), MappedAction::FieldMinus);
        // The latch is released after one key
        QVERIFY(!alt->isChecked());
        minus->click();
        QCOMPARE(chars.count(), 1);
        QCOMPARE(chars.at(0).at(0).value<QChar>(), QChar('-'));
    }

    void testVirtualCtrlUnboundChordTypesNothing() {
        ui::widgets::QVirtualKeyboardWidget keyboard;
        QPushButton *ctrl = findButton(keyboard, QStringLiteral("Ctrl"));
        QPushButton *q = findButton(keyboard, QStringLiteral("q"));
        QVERIFY(ctrl && q);
        QSignalSpy actions(&keyboard, &ui::widgets::QVirtualKeyboardWidget::actionTriggered);
        QSignalSpy chars(&keyboard, &ui::widgets::QVirtualKeyboardWidget::characterTriggered);
        ctrl->click();
        q->click();
        QCOMPARE(chars.count(), 0);
        QCOMPARE(actions.count(), 0);
        QVERIFY(!ctrl->isChecked());
    }

  private:
    static QPushButton *findButton(QWidget &parent, const QString &text) {
        for (QPushButton *b : parent.findChildren<QPushButton *>()) {
            if (b->text() == text) return b;
        }
        return nullptr;
    }
};

QTEST_MAIN(TestRemapEdgeCases)
#include "test_remap_edge_cases.moc"
