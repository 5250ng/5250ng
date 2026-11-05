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
#include "ui/rendering/tn5250_command_handler.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QtTest/QtTest>

using ui::rendering::TN5250CommandHandler;
using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #238: a record holding WTD (CC2 = unlock)
// followed by Write Error Code (a DDS ERRMSG) left the keyboard unlocked,
// because the WTD's CC2 was applied at the end of the record, after the
// Write Error Code had locked it.
class TestWriteErrorCodeLock : public QObject {
    Q_OBJECT

    Q5250ScreenWidget *m_widget = nullptr;
    TN5250CommandHandler *m_handler = nullptr;

    // The order the decoder emits one such record in: WTD control
    // characters, the Write Error Code, then the WTD's display data
    void receiveWtdThenErrorCode(const QByteArray &errorData) {
        m_handler->onControlCharactersReceived(0x00, 0x08);
        m_handler->onWriteErrorCode(errorData);
        m_handler->handleRawScreenData(QByteArray());
    }

    static QByteArray ebcdic(const char *text) { return core::EBCDIC::stringToEBCDIC(text); }

  private slots:
    void init() {
        m_widget = new Q5250ScreenWidget();
        m_widget->setScreenSize(24, 80);
        m_handler = new TN5250CommandHandler(this);
        m_handler->setDisplayWidget(m_widget);
        auto *screen = m_widget->screenBuffer();
        screen->setField(4, 9, 5, false);
        screen->setFieldFFW(4, 9, 0x40, 0x00);
        m_widget->setKeyboardState(KeyboardState::Locked);
    }
    void cleanup() {
        delete m_handler;
        delete m_widget;
    }

    void errorCodeAfterUnlockStaysErrorLocked() {
        receiveWtdThenErrorCode(ebcdic("Error message test 50"));
        QCOMPARE(m_widget->keyboardState(), KeyboardState::ErrorLocked);
        // The cursor stays visible, as on a real terminal
        QVERIFY(m_widget->screenBuffer()->isCursorVisible());
        // Typing is refused until Reset
        auto *screen = m_widget->screenBuffer();
        screen->setCursorPosition(4, 9);
        const uint8_t before = screen->cell(4, 9).character;
        m_widget->dispatchCharacter('x');
        QCOMPARE(screen->cell(4, 9).character, before);
    }

    // The WTD's cursor placement still applies when the error data does
    // not place the cursor: the first input field
    void cursorGoesToFirstFieldWithoutErrorIc() {
        m_widget->screenBuffer()->setCursorPosition(0, 0);
        receiveWtdThenErrorCode(ebcdic("Bad value"));
        QCOMPARE(m_widget->screenBuffer()->cursorPosition(), QPoint(9, 4));
    }

    // An Insert Cursor in the error data wins over the WTD's placement
    void errorCodeInsertCursorIsKept() {
        QByteArray data;
        data.append(char(0x13)).append(char(11)).append(char(21)); // IC row 11, col 21
        data.append(ebcdic("Bad value"));
        receiveWtdThenErrorCode(data);
        QCOMPARE(m_widget->screenBuffer()->cursorPosition(), QPoint(20, 10));
        QCOMPARE(m_widget->keyboardState(), KeyboardState::ErrorLocked);
    }

    // Without a Write Error Code the WTD unlocks as before, and a later
    // record is not affected by the earlier error
    void unlockWithoutErrorCodeIsUnchanged() {
        receiveWtdThenErrorCode(ebcdic("Bad value"));
        m_handler->onControlCharactersReceived(0x00, 0x08);
        m_handler->handleRawScreenData(QByteArray());
        QCOMPARE(m_widget->keyboardState(), KeyboardState::Unlocked);
    }
};

QTEST_MAIN(TestWriteErrorCodeLock)
#include "test_write_error_code_lock.moc"
