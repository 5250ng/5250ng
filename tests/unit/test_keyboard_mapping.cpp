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
#include <QCoreApplication>
#include <QSettings>
#include <QtTest/QtTest>

using namespace core;

class TestKeyboardMapping : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void init();

    void testDefaultsContainExpectedActions();
    void testDefaultPageKeysMapToRollActions();
    void testDefaultHelpPrintClearChords();
    void testShiftF1IsPF13();
    void testCtrlEscapeIsAttn();
    void testSetAndLookup();
    void testSetBindingReplacesOldAction();
    void testClearAction();
    void testKeyChordRoundTripString();
    void testSaveLoadQSettingsRoundTrip();
    void testJsonRoundTrip();
    void testFromJsonInvalidReturnsFalse();
    void testActionNameAndFromName();
    void testChordsForReturnsAllBoundChords();
    void testMultipleChordsCanBindSameAction();
    void testMultipleChordsPersistViaQSettings();

  private:
    void resetMappingToDefaults() {
        KeyboardMapping::instance().resetToDefaults();
    }
};

void TestKeyboardMapping::initTestCase() {
    // Use an isolated QSettings scope so the test never touches user settings.
    QCoreApplication::setOrganizationName("5250ng-test");
    QCoreApplication::setApplicationName("keyboard_mapping_test");
    QSettings settings;
    settings.clear();
}

void TestKeyboardMapping::init() {
    resetMappingToDefaults();
}

void TestKeyboardMapping::testDefaultsContainExpectedActions() {
    auto &m = KeyboardMapping::instance();
    QVERIFY(!m.allBindings().isEmpty());
    // Every PF key should have at least one chord bound by default.
    for (int i = 1; i <= 24; ++i) {
        auto action = static_cast<MappedAction>(
            static_cast<int>(MappedAction::PF1) + (i - 1));
        KeyChord c = m.chordFor(action);
        QVERIFY2(c.isValid(), qPrintable(QString("PF%1 should have a default chord").arg(i)));
    }
}

// Regression test for issue #141: Page Up pages backward (Roll Down),
// Page Down pages forward (Roll Up).
void TestKeyboardMapping::testDefaultPageKeysMapToRollActions() {
    KeyboardMapping &m = KeyboardMapping::instance();
    m.resetToDefaults();
    QCOMPARE(m.lookup(Qt::Key_PageUp, Qt::NoModifier), MappedAction::RollDown);
    QCOMPARE(m.lookup(Qt::Key_PageDown, Qt::NoModifier), MappedAction::RollUp);
}

// Help, Print, and Clear have no dedicated PC key. Their default chords
// exist so script/MCP AID replay works end-to-end; they must stay in sync
// with the reverse map in core/aid_key_map.h.
void TestKeyboardMapping::testDefaultHelpPrintClearChords() {
    auto &m = KeyboardMapping::instance();
    QCOMPARE(m.lookup(Qt::Key_F1, Qt::ControlModifier), MappedAction::Help);
    QCOMPARE(m.lookup(Qt::Key_Print, Qt::NoModifier), MappedAction::Print);
    QCOMPARE(m.lookup(Qt::Key_Pause, Qt::ControlModifier), MappedAction::Clear);
}

void TestKeyboardMapping::testShiftF1IsPF13() {
    auto action = KeyboardMapping::instance().lookup(Qt::Key_F1, Qt::ShiftModifier);
    QCOMPARE(action, MappedAction::PF13);
}

void TestKeyboardMapping::testCtrlEscapeIsAttn() {
    auto action = KeyboardMapping::instance().lookup(Qt::Key_Escape, Qt::ControlModifier);
    QCOMPARE(action, MappedAction::Attn);
}

void TestKeyboardMapping::testSetAndLookup() {
    auto &m = KeyboardMapping::instance();
    KeyChord c{Qt::Key_R, Qt::ControlModifier | Qt::AltModifier};
    m.setBinding(c, MappedAction::Reset);
    QCOMPARE(m.lookup(c), MappedAction::Reset);
}

void TestKeyboardMapping::testSetBindingReplacesOldAction() {
    auto &m = KeyboardMapping::instance();
    KeyChord c{Qt::Key_F1, Qt::ShiftModifier};
    // Default: Shift+F1 -> PF13
    QCOMPARE(m.lookup(c), MappedAction::PF13);
    m.setBinding(c, MappedAction::PF1);
    QCOMPARE(m.lookup(c), MappedAction::PF1);
}

void TestKeyboardMapping::testClearAction() {
    auto &m = KeyboardMapping::instance();
    m.clearAction(MappedAction::PF13);
    QCOMPARE(m.lookup(Qt::Key_F1, Qt::ShiftModifier), MappedAction::None);
    QVERIFY(!m.chordFor(MappedAction::PF13).isValid());
}

void TestKeyboardMapping::testKeyChordRoundTripString() {
    KeyChord c{Qt::Key_F1, Qt::ShiftModifier};
    QString s = c.toString();
    QVERIFY(!s.isEmpty());
    KeyChord back = KeyChord::fromString(s);
    QCOMPARE(back, c);
}

void TestKeyboardMapping::testSaveLoadQSettingsRoundTrip() {
    auto &m = KeyboardMapping::instance();
    // Mutate the table: bind Ctrl+Alt+R to Reset.
    KeyChord c{Qt::Key_R, Qt::ControlModifier | Qt::AltModifier};
    m.setBinding(c, MappedAction::Reset);
    // Remove a default to prove the save replaces, not merges.
    m.clearAction(MappedAction::PF24);

    m.save();

    // Wipe the singleton back to a different state, then reload.
    m.resetToDefaults();
    QCOMPARE(m.lookup(c), MappedAction::None);
    m.load();
    QCOMPARE(m.lookup(c), MappedAction::Reset);
    QVERIFY(!m.chordFor(MappedAction::PF24).isValid());
}

void TestKeyboardMapping::testJsonRoundTrip() {
    auto &m = KeyboardMapping::instance();
    KeyChord c{Qt::Key_Q, Qt::ControlModifier};
    m.setBinding(c, MappedAction::Clear);
    QJsonObject obj = m.toJson();

    KeyboardMapping other;
    other.resetToDefaults();
    QVERIFY(other.fromJson(obj));
    QCOMPARE(other.lookup(c), MappedAction::Clear);
}

void TestKeyboardMapping::testFromJsonInvalidReturnsFalse() {
    KeyboardMapping other;
    QJsonObject bogus;
    bogus["entries"] = QJsonValue(42); // not an array
    QVERIFY(!other.fromJson(bogus));
    QJsonObject missing;
    QVERIFY(!other.fromJson(missing));
}

void TestKeyboardMapping::testActionNameAndFromName() {
    QCOMPARE(KeyboardMapping::actionName(MappedAction::PF13), QStringLiteral("PF13"));
    QCOMPARE(KeyboardMapping::actionFromName("Attn"), MappedAction::Attn);
    QCOMPARE(KeyboardMapping::actionFromName("definitely-not-a-real-action"),
             MappedAction::None);
}

void TestKeyboardMapping::testChordsForReturnsAllBoundChords() {
    // Default: PF13 is bound to both Shift+F1 and F13.
    auto chords = KeyboardMapping::instance().chordsFor(MappedAction::PF13);
    QCOMPARE(chords.size(), 2);
    QVERIFY(chords.contains(KeyChord{Qt::Key_F1, Qt::ShiftModifier}));
    QVERIFY(chords.contains(KeyChord{Qt::Key_F13, Qt::NoModifier}));
}

void TestKeyboardMapping::testMultipleChordsCanBindSameAction() {
    auto &m = KeyboardMapping::instance();
    KeyChord primary{Qt::Key_Q, Qt::ControlModifier};
    KeyChord secondary{Qt::Key_L, Qt::ControlModifier | Qt::AltModifier};
    m.setBinding(primary, MappedAction::Clear);
    m.setBinding(secondary, MappedAction::Clear);
    auto chords = m.chordsFor(MappedAction::Clear);
    QVERIFY(chords.contains(primary));
    QVERIFY(chords.contains(secondary));
    // Both chords resolve to the same action.
    QCOMPARE(m.lookup(primary), MappedAction::Clear);
    QCOMPARE(m.lookup(secondary), MappedAction::Clear);
}

void TestKeyboardMapping::testMultipleChordsPersistViaQSettings() {
    auto &m = KeyboardMapping::instance();
    // Bind to Reset, which has no default chord, so the exact-count check
    // below stays independent of the default table (Clear gained a default
    // chord for script/MCP AID replay).
    KeyChord a{Qt::Key_Q, Qt::ControlModifier};
    KeyChord b{Qt::Key_L, Qt::ControlModifier | Qt::AltModifier};
    m.setBinding(a, MappedAction::Reset);
    m.setBinding(b, MappedAction::Reset);
    m.save();

    m.resetToDefaults();
    // After defaults reset, our custom chords no longer resolve.
    QCOMPARE(m.lookup(a), MappedAction::None);
    QCOMPARE(m.lookup(b), MappedAction::None);

    m.load();
    QCOMPARE(m.lookup(a), MappedAction::Reset);
    QCOMPARE(m.lookup(b), MappedAction::Reset);
    QCOMPARE(m.chordsFor(MappedAction::Reset).size(), 2);
}

QTEST_MAIN(TestKeyboardMapping)
#include "test_keyboard_mapping.moc"
