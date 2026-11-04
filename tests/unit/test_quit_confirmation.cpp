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

#include "ui/quit_confirmation.h"
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QtTest/QtTest>

// Regression tests for issue #409: closing the main window with connected
// sessions dropped them all without asking.
class TestQuitConfirmation : public QObject {
    Q_OBJECT

    // Click `button` on the next modal dialog, recording its text
    void answerNextDialog(const QString &button) {
        m_dialogText.clear();
        m_answered = false;
        QTimer::singleShot(50, this, [this, button] { tryAnswer(button, 0); });
    }

    void tryAnswer(const QString &button, int attempt) {
        QWidget *modal = QApplication::activeModalWidget();
        if (!modal) {
            if (attempt < 100)
                QTimer::singleShot(20, this, [this, button, attempt] { tryAnswer(button, attempt + 1); });
            return;
        }
        for (QLabel *l : modal->findChildren<QLabel *>())
            m_dialogText += l->text() + '\n';
        for (QPushButton *b : modal->findChildren<QPushButton *>()) {
            if (b->text() == button) {
                m_answered = true;
                b->click();
                return;
            }
        }
        // No such button: close the dialog so the test fails instead of hanging
        modal->close();
    }

    QString m_dialogText;
    bool m_answered = false;

  private slots:
    void noConnectedSessionCloses() {
        QWidget parent;
        // No dialog may appear: a stray one would block this call. The timer
        // is tied to `parent` so it cannot fire into a later test.
        QTimer::singleShot(2000, &parent, [] {
            if (QWidget *modal = QApplication::activeModalWidget()) modal->close();
        });
        QVERIFY(ui::confirmQuitWithConnectedSessions(&parent, 0));
        QVERIFY(!QApplication::activeModalWidget());
    }

    void yesQuits() {
        QWidget parent;
        answerNextDialog(QStringLiteral("Yes"));
        QVERIFY(ui::confirmQuitWithConnectedSessions(&parent, 1));
        QVERIFY(m_answered);
        QVERIFY2(m_dialogText.contains(QStringLiteral("1 session is connected")), qPrintable(m_dialogText));
    }

    void noKeepsTheWindowOpen() {
        QWidget parent;
        answerNextDialog(QStringLiteral("No"));
        QVERIFY(!ui::confirmQuitWithConnectedSessions(&parent, 3));
        QVERIFY(m_answered);
        QVERIFY2(m_dialogText.contains(QStringLiteral("3 sessions are connected")), qPrintable(m_dialogText));
    }
};

QTEST_MAIN(TestQuitConfirmation)
#include "test_quit_confirmation.moc"
