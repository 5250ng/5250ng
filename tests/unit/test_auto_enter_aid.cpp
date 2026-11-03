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

#include "core/ebcdic.h"
#include "core/keyboard_mapping.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// An auto-enter field (FFW2 0x80, DDS CHECK(ER)) performs the Enter
// function: the host must receive AID 0xF1, not 0x3F (the selector light
// pen auto-enter AID, which keyboard input must never send).
class TestAutoEnterAid : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        m_widget = new Q5250ScreenWidget();
        m_widget->setScreenSize(24, 80);
        auto *screen = m_widget->screenBuffer();
        screen->setField(2, 19, 3, false);
        screen->setFieldFFW(2, 19, 0x40, 0x80); // auto-enter
        screen->setField(4, 19, 3, false);
        screen->setFieldFFW(4, 19, 0x40, 0x00);
        screen->setCursorPosition(2, 19);
        m_widget->setKeyboardState(KeyboardState::Unlocked);
    }
    void cleanup() { delete m_widget; }

    void fillingTheFieldSendsEnter() {
        QSignalSpy spy(m_widget, &Q5250ScreenWidget::inputReady);
        for (QChar ch : QString("abc"))
            m_widget->dispatchCharacter(ch);
        QCOMPARE(spy.count(), 1);
        const QByteArray sent = spy.at(0).at(0).toByteArray();
        QVERIFY(sent.size() >= 3);
        QCOMPARE(quint8(sent[2]), quint8(0xF1));
        QVERIFY(sent.contains(core::EBCDIC::stringToEBCDIC("abc")));
        QCOMPARE(m_widget->keyboardState(), KeyboardState::Locked);
    }

    void fieldExitOnAutoEnterFieldSendsEnter_data() {
        QTest::addColumn<int>("action");
        QTest::newRow("FieldExit") << int(core::MappedAction::FieldExit);
        QTest::newRow("FieldPlus") << int(core::MappedAction::FieldPlus);
        QTest::newRow("FieldMinus") << int(core::MappedAction::FieldMinus);
    }

    void fieldExitOnAutoEnterFieldSendsEnter() {
        QFETCH(int, action);
        QSignalSpy spy(m_widget, &Q5250ScreenWidget::inputReady);
        m_widget->dispatchCharacter('a');
        QCOMPARE(spy.count(), 0);
        m_widget->dispatchMappedAction(static_cast<core::MappedAction>(action));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(quint8(spy.at(0).at(0).toByteArray()[2]), quint8(0xF1));
    }

    // A normal field still advances to the next field and sends nothing.
    void fieldExitOnNormalFieldDoesNotEnter() {
        m_widget->screenBuffer()->setCursorPosition(4, 19);
        QSignalSpy spy(m_widget, &Q5250ScreenWidget::inputReady);
        m_widget->dispatchCharacter('a');
        m_widget->dispatchMappedAction(core::MappedAction::FieldExit);
        QCOMPARE(spy.count(), 0);
    }

  private:
    Q5250ScreenWidget *m_widget = nullptr;
};

QTEST_MAIN(TestAutoEnterAid)
#include "test_auto_enter_aid.moc"
