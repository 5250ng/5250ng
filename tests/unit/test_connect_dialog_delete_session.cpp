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
#include "session/manager.h"
#include "ui/dialogs/connect_dialog.h"
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QtTest/QtTest>

// Deleting the selected session must leave a fresh form, not the next
// session's settings shown as an unsaved new session.
class TestConnectDialogDeleteSession : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    // Runs even when an assertion failed.
    void cleanup() {
        session::SessionManager mgr;
        mgr.deleteSession("del-a");
        mgr.deleteSession("del-b");
    }

    void deleteLeavesAFreshForm() {
        session::SessionManager mgr;
        for (const auto &[name, host] : {std::pair<QString, QString>{"del-a", "a.example"},
                                         std::pair<QString, QString>{"del-b", "b.example"}}) {
            session::SessionConfig c;
            c.setName(name);
            c.setHostname(host);
            c.setUsername(name.toUpper());
            c.setPassword(QStringLiteral("secret-") + name);
            QVERIFY(mgr.saveSession(c));
        }

        ConnectDialog dialog;
        QComboBox *combo = nullptr;
        for (QComboBox *c : dialog.findChildren<QComboBox *>()) {
            if (c->findText("(New Session)") >= 0)
                combo = c;
        }
        QVERIFY(combo);
        combo->setCurrentText("del-a");
        QCOMPARE(dialog.getSessionConfig().hostname(), QString("a.example"));

        // Answer "Delete session?" with Yes and dismiss the confirmation.
        QTimer answer;
        connect(&answer, &QTimer::timeout, this, []() {
            QWidget *modal = QApplication::activeModalWidget();
            if (!modal) return;
            for (QPushButton *b : modal->findChildren<QPushButton *>()) {
                if (b->text() == "Yes" || b->text() == "OK") {
                    b->click();
                    return;
                }
            }
        });
        answer.start(20);
        QPushButton *del = nullptr;
        for (QPushButton *b : dialog.findChildren<QPushButton *>()) {
            if (b->text() == "Delete Session")
                del = b;
        }
        QVERIFY(del);
        del->click();
        answer.stop();

        QCOMPARE(combo->currentText(), QString("(New Session)"));
        QCOMPARE(combo->findText("del-a"), -1);
        QVERIFY2(dialog.getSessionConfig().hostname() != "b.example",
                 "the next session was loaded into the form");
        QVERIFY(dialog.getSessionConfig().hostname().isEmpty());
        // Nor are the neighbour's credentials carried into the new session.
        QVERIFY(dialog.getSessionConfig().username().isEmpty());
        QVERIFY(dialog.getSessionConfig().password().isEmpty());
        QVERIFY(!del->isEnabled());
    }
};

QTEST_MAIN(TestConnectDialogDeleteSession)
#include "test_connect_dialog_delete_session.moc"
