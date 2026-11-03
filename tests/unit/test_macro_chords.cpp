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
#include "core/macro_recorder.h"
#include "core/macro_to_script.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest/QtTest>

using core::Macro;
using core::MacroStep;
using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// A recorded macro must replay what the operator did: chords the keyboard
// mapping turns into 5250 actions are written as those actions.
class TestMacroChords : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        core::KeyboardMapping::instance().resetToDefaults();
    }

    void chordsBecomeTheirAction_data() {
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("mods");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("line");
        QTest::newRow("Ctrl+End") << int(Qt::Key_End) << int(Qt::ControlModifier) << "" << "PRESS FIELDEXIT";
        QTest::newRow("Ctrl+Del") << int(Qt::Key_Delete) << int(Qt::ControlModifier) << "" << "PRESS ERASEEOF";
        QTest::newRow("Shift+Ins") << int(Qt::Key_Insert) << int(Qt::ShiftModifier) << "" << "PRESS DUP";
        QTest::newRow("Alt++") << int(Qt::Key_Plus) << int(Qt::AltModifier) << "+" << "PRESS FIELDPLUS";
        QTest::newRow("Shift+Tab") << int(Qt::Key_Tab) << int(Qt::ShiftModifier) << "" << "MOVE CURSOR AT PREVIOUS INPUTFIELD";
        QTest::newRow("End") << int(Qt::Key_End) << int(Qt::NoModifier) << "" << "PRESS END";
    }

    void chordsBecomeTheirAction() {
        QFETCH(int, key);
        QFETCH(int, mods);
        QFETCH(QString, text);
        QFETCH(QString, line);
        const QStringList lines = script({keyPress(key, mods, text)});
        QVERIFY2(lines.contains(line), qPrintable(lines.join(" | ")));
    }

    // AID chords are written once, from the AID step, and leave no
    // "# Unknown key" comments.
    void aidChordsComeFromTheAidStep() {
        MacroStep aid;
        aid.type = MacroStep::AIDKey;
        aid.aidByte = 0x33; // PF3
        const QStringList lines = script({keyPress(Qt::Key_F3, Qt::NoModifier, ""), aid});
        QCOMPARE(lines.filter(QRegularExpression("^PRESS ")).size(), 1);
        QVERIFY(!lines.join('\n').contains("Unknown key"));
    }

    void typedTextIsKept() {
        const QStringList lines = script({keyPress(Qt::Key_A, Qt::NoModifier, "a"),
                                          keyPress(Qt::Key_B, Qt::ShiftModifier, "B")});
        QVERIFY(lines.contains("TYPE \"aB\""));
    }

    // Attn and SysReq never reached the recorder
    void attnAndSysReqAreRecorded() {
        Q5250ScreenWidget widget;
        widget.setScreenSize(24, 80);
        widget.setKeyboardState(KeyboardState::Unlocked);
        QSignalSpy aids(&widget, &Q5250ScreenWidget::aidKeyRecorded);
        QTest::keyClick(&widget, Qt::Key_Escape, Qt::ControlModifier); // Attn
        QTest::keyClick(&widget, Qt::Key_SysReq);
        QCOMPARE(aids.size(), 2);
        QCOMPARE(aids.at(0).at(0).value<uint8_t>(), uint8_t(0x70));
        QCOMPARE(aids.at(1).at(0).value<uint8_t>(), uint8_t(0x71));
        MacroStep attn;
        attn.type = MacroStep::AIDKey;
        attn.aidByte = 0x70;
        QVERIFY(script({keyPress(Qt::Key_Escape, Qt::ControlModifier, ""), attn}).contains("PRESS ATTN"));
    }

  private:
    static MacroStep keyPress(int key, int mods, const QString &text) {
        MacroStep s;
        s.type = MacroStep::KeyPress;
        s.key = key;
        s.mods = Qt::KeyboardModifiers(mods);
        s.text = text;
        return s;
    }
    static QStringList script(const QVector<MacroStep> &steps) {
        Macro m;
        m.name = QStringLiteral("chords");
        m.steps = steps;
        return core::macroToScript(m).split('\n');
    }
};

QTEST_MAIN(TestMacroChords)
#include "test_macro_chords.moc"
