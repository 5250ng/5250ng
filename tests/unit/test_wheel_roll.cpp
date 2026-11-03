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
#include <QSignalSpy>
#include <QWheelEvent>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// The wheel sends one Roll per notch (120 units of angleDelta), however the
// device splits it into events.
class TestWheelRoll : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        m_widget = new Q5250ScreenWidget();
        m_widget->setScreenSize(24, 80);
        m_widget->setKeyboardState(KeyboardState::Unlocked);
        m_spy = new QSignalSpy(m_widget, &Q5250ScreenWidget::inputReady);
    }
    void cleanup() {
        delete m_spy;
        delete m_widget;
    }

    void oneNotchInSmallStepsIsOneRoll() {
        for (int i = 0; i < 8; ++i)
            wheel(15);
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(aid(0), quint8(0xF4)); // Roll Down (Page Up)
    }

    void partialNotchSendsNothing() {
        for (int i = 0; i < 7; ++i)
            wheel(-15);
        QCOMPARE(m_spy->count(), 0);
    }

    void classicMouseNotchIsOneRoll() {
        wheel(-120);
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(aid(0), quint8(0xF5)); // Roll Up (Page Down)
    }

    void directionChangeRestartsTheNotch() {
        wheel(100);
        wheel(-100);
        wheel(-19);
        QCOMPARE(m_spy->count(), 0);
        wheel(-1);
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(aid(0), quint8(0xF5));
    }

    // Scrolling while the host has the keyboard locked (after a Roll) is
    // dropped instead of paging again as soon as it unlocks.
    void scrollWhileLockedIsDropped() {
        wheel(120);
        QCOMPARE(m_spy->count(), 1);
        QCOMPARE(m_widget->keyboardState(), KeyboardState::Locked);
        // An odd number of half notches: without the lock check a partial
        // notch would still be pending when the keyboard unlocks.
        for (int i = 0; i < 3; ++i)
            wheel(60);
        m_widget->setKeyboardState(KeyboardState::Unlocked);
        wheel(60);
        QCOMPARE(m_spy->count(), 1);
    }

    void newGestureRestartsTheNotch() {
        wheel(100, Qt::NoModifier, Qt::ScrollUpdate);
        wheel(0, Qt::NoModifier, Qt::ScrollEnd);
        wheel(0, Qt::NoModifier, Qt::ScrollBegin);
        wheel(100, Qt::NoModifier, Qt::ScrollUpdate);
        QCOMPARE(m_spy->count(), 0);
    }

    void ctrlWheelIsNotARoll() {
        wheel(120, Qt::ControlModifier);
        QCOMPARE(m_spy->count(), 0);
    }

  private:
    void wheel(int dy, Qt::KeyboardModifiers mods = Qt::NoModifier,
               Qt::ScrollPhase phase = Qt::NoScrollPhase) {
        QWheelEvent ev(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, dy),
                       Qt::NoButton, mods, phase, false);
        QCoreApplication::sendEvent(m_widget, &ev);
    }
    quint8 aid(int i) const { return quint8(m_spy->at(i).at(0).toByteArray().at(2)); }

    Q5250ScreenWidget *m_widget = nullptr;
    QSignalSpy *m_spy = nullptr;
};

QTEST_MAIN(TestWheelRoll)
#include "test_wheel_roll.moc"
