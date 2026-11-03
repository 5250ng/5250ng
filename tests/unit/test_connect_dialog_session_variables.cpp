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

#include "session/config.h"
#include "ui/dialogs/connect_dialog.h"
#include <QApplication>
#include <QLineEdit>
#include <QStandardPaths>
#include <QtTest/QtTest>


// A session-variable field left blank in the Connect dialog must not be
// saved as a defined (empty) variable, or "IF ISSET $SESSION_..." in the
// startup script is true and its fallback never runs.
class TestConnectDialogSessionVariables : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void blankFieldsAreNotSaved() {
        session::SessionConfig config;
        config.setName(QStringLiteral("vars"));
        config.setHostname(QStringLiteral("ibmi.example"));
        config.setStartupScriptSource(QStringLiteral(
            "IF ISSET $SESSION_USERNAME\n"
            "    TYPE \"$SESSION_USERNAME\"\n"
            "ENDIF\n"
            "IF ISSET $SESSION_PASSWORD\n"
            "    TYPE \"$SESSION_PASSWORD\"\n"
            "ENDIF\n"));
        config.setSessionVariables({{QStringLiteral("$SESSION_USERNAME"), QString()},
                                    {QStringLiteral("$SESSION_PASSWORD"), QStringLiteral("pw")}});

        ConnectDialog dialog;
        dialog.setSessionConfig(config);
        const QHash<QString, QString> vars = dialog.getSessionConfig().sessionVariables();
        QVERIFY2(!vars.contains(QStringLiteral("$SESSION_USERNAME")),
                 "blank field saved as a defined variable");
        QCOMPARE(vars.value(QStringLiteral("$SESSION_PASSWORD")), QStringLiteral("pw"));
    }
};

QTEST_MAIN(TestConnectDialogSessionVariables)
#include "test_connect_dialog_session_variables.moc"
