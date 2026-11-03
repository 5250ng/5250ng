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
#include <QApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest/QtTest>

using core::KeyboardMapping;
using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// A chord removed in Remap Keyboard must do nothing when typed, instead of
// falling back to the encoder's built-in meaning (F12 -> PF12, ...).
class TestUnboundKey : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init() {
        KeyboardMapping::instance().resetToDefaults();
        m_widget = new Q5250ScreenWidget();
        m_widget->setScreenSize(24, 80);
        m_widget->setKeyboardState(KeyboardState::Unlocked);
        m_widget->show();
        m_spy = new QSignalSpy(m_widget, &Q5250ScreenWidget::inputReady);
    }

    void cleanup() {
        delete m_spy;
        delete m_widget;
        KeyboardMapping::instance().resetToDefaults();
    }

    void unboundKeyIsIgnored_data() {
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("mods");
        QTest::newRow("F12") << int(Qt::Key_F12) << int(Qt::NoModifier);
        QTest::newRow("Shift+F1") << int(Qt::Key_F1) << int(Qt::ShiftModifier);
        QTest::newRow("Enter") << int(Qt::Key_Return) << int(Qt::NoModifier);
        QTest::newRow("PageDown") << int(Qt::Key_PageDown) << int(Qt::NoModifier);
    }

    void unboundKeyIsIgnored() {
        QFETCH(int, key);
        QFETCH(int, mods);
        const auto modifiers = Qt::KeyboardModifiers(mods);
        QVERIFY(KeyboardMapping::instance().lookup(key, modifiers) != core::MappedAction::None);
        KeyboardMapping::instance().clearChord({key, modifiers});
        QTest::keyClick(m_widget, Qt::Key(key), modifiers);
        QCOMPARE(m_spy->count(), 0);
    }

    // Keys that are still bound keep working.
    void boundKeyStillWorks() {
        KeyboardMapping::instance().clearChord({Qt::Key_F12, Qt::NoModifier});
        QTest::keyClick(m_widget, Qt::Key_F3);
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(quint8(m_spy->first().at(0).toByteArray().at(2)), quint8(0x33)); // PF3
    }

    // With the default mapping, modifier variants of a bound key keep
    // working (Shift held while typing, then Enter).
    void modifierVariantFollowsBaseKey_data() {
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("mods");
        QTest::addColumn<int>("aid");
        QTest::newRow("Shift+Enter") << int(Qt::Key_Return) << int(Qt::ShiftModifier) << 0xF1;
        QTest::newRow("Shift+PageDown") << int(Qt::Key_PageDown) << int(Qt::ShiftModifier) << 0xF5;
        QTest::newRow("Ctrl+F5") << int(Qt::Key_F5) << int(Qt::ControlModifier) << 0x35;
        QTest::newRow("Ctrl+Shift+F2") << int(Qt::Key_F2) << int(Qt::ControlModifier | Qt::ShiftModifier) << 0xB2;
    }

    void modifierVariantFollowsBaseKey() {
        QFETCH(int, key);
        QFETCH(int, mods);
        QFETCH(int, aid);
        QTest::keyClick(m_widget, Qt::Key(key), Qt::KeyboardModifiers(mods));
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(int(quint8(m_spy->first().at(0).toByteArray().at(2))), aid);
    }

    // The variant follows the base key's current binding: it stops when the
    // base key is unbound and moves with a remap.
    void modifierVariantFollowsRemap() {
        auto &mapping = KeyboardMapping::instance();
        mapping.clearChord({Qt::Key_F5, Qt::NoModifier});
        QTest::keyClick(m_widget, Qt::Key_F5, Qt::ControlModifier);
        QCOMPARE(m_spy->count(), 0);
        mapping.setBinding({Qt::Key_F5, Qt::NoModifier}, core::MappedAction::PF10);
        QTest::keyClick(m_widget, Qt::Key_F5, Qt::ControlModifier);
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(quint8(m_spy->first().at(0).toByteArray().at(2)), quint8(0x3A)); // PF10
    }

    // Linux reports the SysReq key as Alt+SysReq.
    void altSysReqIsSystemRequest() {
        QSignalSpy sysreq(m_widget, &Q5250ScreenWidget::systemRequestRequested);
        QTest::keyClick(m_widget, Qt::Key_SysReq, Qt::AltModifier);
        QCOMPARE(sysreq.count(), 1);
    }

    // Scripts and MCP send synthetic key events for PRESS F12; those keep
    // the key's built-in meaning whatever the user's mapping.
    void syntheticKeyStillSendsAid() {
        KeyboardMapping::instance().clearChord({Qt::Key_F12, Qt::NoModifier});
        QKeyEvent ev(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);
        QApplication::sendEvent(m_widget, &ev);
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(quint8(m_spy->first().at(0).toByteArray().at(2)), quint8(0x3C)); // PF12
    }

  private:
    Q5250ScreenWidget *m_widget = nullptr;
    QSignalSpy *m_spy = nullptr;
};

QTEST_MAIN(TestUnboundKey)
#include "test_unbound_key.moc"
