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
#include <5250script/screen_interface.h>
#include <5250script/script_executor.h>
#include <5250script/script_parser.h>
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace core;
using namespace core::scripting;

namespace {

// Minimal unlocked 24x80 screen; PRESS of a local key never reads it.
class FakeScreen : public ScreenInterface {
  public:
    int rows() const override { return 24; }
    int cols() const override { return 80; }
    int cursorRow() const override { return 0; }
    int cursorCol() const override { return 0; }
    QString readText(int, int, int length) const override { return QString(length, ' '); }
    QString readFieldText(int, int) const override { return QString(); }
    KeyboardState keyboardState() const override { return KeyboardState::Unlocked; }
    bool messageWaiting() const override { return false; }
};

struct InjectedKey {
    int key = 0;
    Qt::KeyboardModifiers mods = Qt::NoModifier;
};

// Run "PRESS <name>" through ScriptExecutor and return the single key event
// it asks the host to inject.
bool injectedKeyFor(const QString &name, InjectedKey &out) {
    ScriptParser parser;
    ParseResult parsed = parser.parse(QStringLiteral("PRESS ") + name);
    if (!parsed.errors.isEmpty()) return false;

    FakeScreen screen;
    ScriptExecutor executor;
    executor.setScreen(&screen);

    QList<InjectedKey> keys;
    QObject::connect(&executor, &ScriptExecutor::injectKeyPress,
                     [&keys](int key, Qt::KeyboardModifiers mods, const QString &) {
                         keys.append({key, mods});
                     });
    QSignalSpy finished(&executor, &ScriptExecutor::executionFinished);
    executor.execute(parsed);
    if (finished.isEmpty() && !finished.wait(2000)) return false;
    if (keys.size() != 1) return false;
    out = keys.first();
    return true;
}

} // namespace

class TestScriptLocalKeys : public QObject {
    Q_OBJECT

  private slots:
    void testLocalKeysResolveThroughDefaultMapping_data();
    void testLocalKeysResolveThroughDefaultMapping();
    void testEscapeAliasesSendErrorReset_data();
    void testEscapeAliasesSendErrorReset();
};

void TestScriptLocalKeys::testLocalKeysResolveThroughDefaultMapping_data() {
    QTest::addColumn<QString>("name");
    QTest::addColumn<MappedAction>("expected");

    QTest::newRow("TAB")        << "TAB"        << MappedAction::Tab;
    QTest::newRow("BACKTAB")    << "BACKTAB"    << MappedAction::BackTab;
    QTest::newRow("BACKSPACE")  << "BACKSPACE"  << MappedAction::Backspace;
    QTest::newRow("DELETE")     << "DELETE"     << MappedAction::Delete;
    QTest::newRow("INSERT")     << "INSERT"     << MappedAction::Insert;
    QTest::newRow("HOME")       << "HOME"       << MappedAction::Home;
    QTest::newRow("END")        << "END"        << MappedAction::End;
    QTest::newRow("FIELDPLUS")  << "FIELDPLUS"  << MappedAction::FieldPlus;
    QTest::newRow("FIELDMINUS") << "FIELDMINUS" << MappedAction::FieldMinus;
    QTest::newRow("FIELDEXIT")  << "FIELDEXIT"  << MappedAction::FieldExit;
    QTest::newRow("DUP")        << "DUP"        << MappedAction::Dup;
    QTest::newRow("ERASEINPUT") << "ERASEINPUT" << MappedAction::EraseInput;
    QTest::newRow("ERASEFIELD") << "ERASEFIELD" << MappedAction::EraseField;
    QTest::newRow("ERASEEOF")   << "ERASEEOF"   << MappedAction::EraseEOF;
}

// Every local key a script presses must reach the widget as a chord that
// Q5250ScreenWidget::keyPressEvent resolves, through KeyboardMapping's
// defaults, to the matching action. The widget masks the chord to
// Shift/Ctrl/Alt/Meta before the lookup (events.cpp), so do the same here.
void TestScriptLocalKeys::testLocalKeysResolveThroughDefaultMapping() {
    QFETCH(QString, name);
    QFETCH(MappedAction, expected);

    InjectedKey injected;
    QVERIFY2(injectedKeyFor(name, injected),
             qPrintable(QString("PRESS %1 did not inject exactly one key").arg(name)));

    KeyboardMapping mapping; // constructor installs the default chord table
    Qt::KeyboardModifiers mods = injected.mods &
        (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    MappedAction action = mapping.lookup(injected.key, mods);
    QVERIFY2(action == expected,
             qPrintable(QString("PRESS %1 injected key 0x%2 mods 0x%3 -> %4, expected %5")
                            .arg(name)
                            .arg(injected.key, 0, 16)
                            .arg(static_cast<int>(injected.mods), 0, 16)
                            .arg(KeyboardMapping::actionName(action),
                                 KeyboardMapping::actionName(expected))));
}

void TestScriptLocalKeys::testEscapeAliasesSendErrorReset_data() {
    QTest::addColumn<QString>("name");
    QTest::newRow("ESC")    << "ESC";
    QTest::newRow("ESCAPE") << "ESCAPE";
}

// Error Reset is handled by keyPressEvent on a bare Escape before the
// mapping lookup, so both spellings must inject exactly that.
void TestScriptLocalKeys::testEscapeAliasesSendErrorReset() {
    QFETCH(QString, name);

    InjectedKey injected;
    QVERIFY2(injectedKeyFor(name, injected),
             qPrintable(QString("PRESS %1 did not inject exactly one key").arg(name)));
    QCOMPARE(injected.key, static_cast<int>(Qt::Key_Escape));
    QCOMPARE(injected.mods, Qt::KeyboardModifiers(Qt::NoModifier));
}

QTEST_MAIN(TestScriptLocalKeys)
#include "test_script_local_keys.moc"
