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

#include "core/system_name.h"
#include <QtTest/QtTest>

// The status bar shows the system name read from the IBM i sign-on screen.
class TestSystemName : public QObject {
    Q_OBJECT

  private slots:
    void signOnScreen() {
        // Rows as V7R5 sends them
        const QStringList rows = {
            QString(35, ' ') + "Sign On",
            QString(47, ' ') + "System  . . . . . :   RSRCH09",
            QString(47, ' ') + "Subsystem . . . . :   QBASE",
            QString(47, ' ') + "Display . . . . . :   QPADEV0001",
        };
        QCOMPARE(core::signOnSystemName(rows), QString("RSRCH09"));
    }

    void namesWithSpecialCharacters() {
        QCOMPARE(core::signOnSystemName({"   System  . . . . . :   SYS$"}), QString("SYS$"));
        QCOMPARE(core::signOnSystemName({"   System  . . . . . :   @SYS#   "}), QString("@SYS#"));
        QCOMPARE(core::signOnSystemName({"   System  . . . . . :   TOOLONGNAME"}), QString());
    }

    void otherScreens() {
        QCOMPARE(core::signOnSystemName({"                         MAIN                   IBM i Main Menu",
                                         "                                                System:   RSRCH09"}),
                 QString());  // the main menu's "System:" is not the sign-on label
        QCOMPARE(core::signOnSystemName({"Subsystem . . . . :   QBASE"}), QString());
        QCOMPARE(core::signOnSystemName({}), QString());
    }

    void onlyTheTopOfTheScreenIsRead() {
        QStringList rows(10, QString());
        rows[9] = "System  . . . . . :   LATE";
        QCOMPARE(core::signOnSystemName(rows), QString());
    }
};

QTEST_GUILESS_MAIN(TestSystemName)
#include "test_system_name.moc"
