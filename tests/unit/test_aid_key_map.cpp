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

#include "core/aid_key_map.h"
#include "core/keyboard_encoder.h"
#include "core/keyboard_mapping.h"
#include <QKeyEvent>
#include <QList>
#include <QtTest/QtTest>

using namespace core;

namespace {

// Every AID byte aidToQtKey() accepts.
QList<int> acceptedAids() {
    QList<int> aids;
    aids << 0xF1 << 0x70 << 0x71;                 // Enter, Attn, SysReq
    for (int a = 0x31; a <= 0x3C; ++a) aids << a; // PF1..PF12
    for (int a = 0xB1; a <= 0xBC; ++a) aids << a; // PF13..PF24
    aids << 0xF3 << 0xF4 << 0xF5 << 0xF6 << 0xBD; // Help, RollDown, RollUp, Print, Clear
    return aids;
}

// Mirror of Q5250ScreenWidget::dispatchMappedAction() (events.cpp): how the
// widget turns a resolved MappedAction into the AID byte sent to the host.
// PF keys and host actions go through KeyboardEncoder; Help has no
// KeyboardAction enum member yet, so the widget appends the raw Help AID
// (0xF3) itself.
QByteArray encodeLikeDispatchMappedAction(KeyboardEncoder &encoder, MappedAction action) {
    if (action >= MappedAction::PF1 && action <= MappedAction::PF24) {
        int pfNumber = static_cast<int>(action) - static_cast<int>(MappedAction::PF1) + 1;
        return encoder.encodePFKey(pfNumber);
    }
    switch (action) {
    case MappedAction::Enter:    return encoder.encodeAction(KeyboardAction::Enter);
    case MappedAction::Clear:    return encoder.encodeAction(KeyboardAction::Clear);
    case MappedAction::RollUp:   return encoder.encodeAction(KeyboardAction::RollUp);
    case MappedAction::RollDown: return encoder.encodeAction(KeyboardAction::RollDown);
    case MappedAction::Attn:     return encoder.encodeAction(KeyboardAction::Attn);
    case MappedAction::SysReq:   return encoder.encodeAction(KeyboardAction::SysReq);
    case MappedAction::Print:    return encoder.encodeAction(KeyboardAction::Print);
    case MappedAction::Help:     return QByteArray(1, static_cast<char>(0xF3));
    default:                     return QByteArray();
    }
}

} // namespace

class TestAidKeyMap : public QObject {
    Q_OBJECT

  private slots:
    void testRoundTripThroughDefaultMappingAndEncoder();
    void testRoundTripThroughEncodeKeyEvent();
    void testRollAidsAreInverseOfEncoder();
    void testRejectsUnhandledAids();
};

// Round-trip identity over the path script/MCP AID replay actually takes:
// aidToQtKey() -> Q5250ScreenWidget::keyPressEvent resolves the chord via
// KeyboardMapping's defaults -> dispatchMappedAction encodes the action
// through KeyboardEncoder. For every accepted AID byte the result must be
// the original byte.
void TestAidKeyMap::testRoundTripThroughDefaultMappingAndEncoder() {
    KeyboardMapping mapping; // constructor installs the default chord table
    KeyboardEncoder encoder;

    const QList<int> aids = acceptedAids();
    QCOMPARE(aids.size(), 32);

    for (int aidInt : aids) {
        const uint8_t aid = static_cast<uint8_t>(aidInt);
        int qtKey = 0;
        Qt::KeyboardModifiers mods = Qt::NoModifier;
        QVERIFY2(aidToQtKey(aid, qtKey, mods),
                 qPrintable(QString("aidToQtKey rejected AID 0x%1")
                                .arg(aidInt, 2, 16, QChar('0'))));

        MappedAction action = mapping.lookup(qtKey, mods);
        QVERIFY2(action != MappedAction::None,
                 qPrintable(QString("no default chord binding for AID 0x%1 (key 0x%2)")
                                .arg(aidInt, 2, 16, QChar('0'))
                                .arg(qtKey, 0, 16)));

        QByteArray encoded = encodeLikeDispatchMappedAction(encoder, action);
        QCOMPARE(encoded.size(), 1);
        QVERIFY2(static_cast<uint8_t>(encoded[0]) == aid,
                 qPrintable(QString("AID 0x%1 round-tripped to 0x%2")
                                .arg(aidInt, 2, 16, QChar('0'))
                                .arg(static_cast<uint8_t>(encoded[0]), 2, 16, QChar('0'))));
    }
}

// KeyboardEncoder::encodeKeyEvent() also understands most of these chords
// directly (the unmapped-chord fallback path in processKeyEvent). Help,
// Print, and Clear are excluded: their chords exist only as KeyboardMapping
// bindings, and encodeKeyEvent() has no case for them.
void TestAidKeyMap::testRoundTripThroughEncodeKeyEvent() {
    KeyboardEncoder encoder;

    for (int aidInt : acceptedAids()) {
        if (aidInt == 0xF3 || aidInt == 0xF6 || aidInt == 0xBD) continue;

        const uint8_t aid = static_cast<uint8_t>(aidInt);
        int qtKey = 0;
        Qt::KeyboardModifiers mods = Qt::NoModifier;
        QVERIFY(aidToQtKey(aid, qtKey, mods));

        QKeyEvent event(QEvent::KeyPress, qtKey, mods);
        QByteArray encoded = encoder.encodeKeyEvent(&event,
                                                    mods.testFlag(Qt::ShiftModifier),
                                                    mods.testFlag(Qt::ControlModifier),
                                                    mods.testFlag(Qt::AltModifier));

        QCOMPARE(encoded.size(), 1);
        QVERIFY2(static_cast<uint8_t>(encoded[0]) == aid,
                 qPrintable(QString("AID 0x%1 round-tripped to 0x%2 via encodeKeyEvent")
                                .arg(aidInt, 2, 16, QChar('0'))
                                .arg(static_cast<uint8_t>(encoded[0]), 2, 16, QChar('0'))));
    }
}

// Regression guard for issue #141 / the reverse maps missed by PR #142:
// per SA21-9247 window-movement semantics, Roll Down (0xF4) shows the
// previous page and Roll Up (0xF5) the next page, so 0xF4 must map back to
// Page Up and 0xF5 to Page Down — the exact inverse of KeyboardEncoder.
void TestAidKeyMap::testRollAidsAreInverseOfEncoder() {
    int qtKey = 0;
    Qt::KeyboardModifiers mods = Qt::NoModifier;

    QVERIFY(aidToQtKey(0xF4, qtKey, mods));
    QCOMPARE(qtKey, static_cast<int>(Qt::Key_PageUp));
    QCOMPARE(mods, Qt::KeyboardModifiers(Qt::NoModifier));

    QVERIFY(aidToQtKey(0xF5, qtKey, mods));
    QCOMPARE(qtKey, static_cast<int>(Qt::Key_PageDown));
    QCOMPARE(mods, Qt::KeyboardModifiers(Qt::NoModifier));
}

void TestAidKeyMap::testRejectsUnhandledAids() {
    // Bytes bracketing every accepted range plus AIDs with no key
    // equivalent (0x3F Auto Enter, 0xF8 Record Backspace).
    const int rejected[] = {0x00, 0x20, 0x30, 0x3D, 0x3F, 0x6F, 0x72,
                            0xB0, 0xBE, 0xF0, 0xF2, 0xF7, 0xF8, 0xFF};
    for (int aidInt : rejected) {
        int qtKey = 123;
        Qt::KeyboardModifiers mods = Qt::ShiftModifier;
        QVERIFY2(!aidToQtKey(static_cast<uint8_t>(aidInt), qtKey, mods),
                 qPrintable(QString("AID 0x%1 should be rejected")
                                .arg(aidInt, 2, 16, QChar('0'))));
        // Outputs are reset even on rejection.
        QCOMPARE(qtKey, 0);
        QCOMPARE(mods, Qt::KeyboardModifiers(Qt::NoModifier));
    }
}

QTEST_MAIN(TestAidKeyMap)
#include "test_aid_key_map.moc"
