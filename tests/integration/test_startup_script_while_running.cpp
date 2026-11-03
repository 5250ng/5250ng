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
#include "ui/main_window.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"

#include <QKeyEvent>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// Regression test for issue #303: a startup script that started while
// another script was running on the session rewired the executor (so the
// running script's keys reached the screen twice) and was then refused by
// execute(), so it never ran.
class TestStartupScriptWhileRunning : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void startupScriptWaitsForRunningScript() { runScenario(false); }
    void stopCancelsQueuedStartupScript() { runScenario(true); }

  private:
    void runScenario(bool stopManualScript) {
        QTcpServer host;
        int connections = 0;
        QVERIFY(host.listen(QHostAddress::LocalHost, 0));
        connect(&host, &QTcpServer::newConnection, this, [&host, &connections]() {
            while (QTcpSocket *socket = host.nextPendingConnection()) {
                ++connections;
                socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
            }
        });

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString manualPath = dir.filePath(QStringLiteral("manual.5250script"));
        {
            QFile f(manualPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("TYPE \"a\"\nWAIT 1500\nTYPE \"b\"\n");
        }

        // The startup script runs about 500 ms after connecting, while the
        // manual script started right after connecting is in its WAIT.
        session::SessionConfig config;
        config.setName(QStringLiteral("startup"));
        config.setHostname(QStringLiteral("127.0.0.1"));
        config.setPort(host.serverPort());
        config.setStartupScriptTrigger(session::StartupScriptTrigger::AfterConnect);
        config.setStartupScriptName(QStringLiteral("startup.5250script"));
        config.setStartupScriptSource(QStringLiteral("TYPE \"S\"\n"));

        MainWindow window;
        window.autoConnect(config);
        auto *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 1, 5000);
        auto *display = window.findChild<ui::widgets::Q5250ScreenWidget *>();
        QVERIFY(display);

        // Record every character the scripts deliver to the screen
        QString typed;
        struct KeyLog : QObject {
            QString *out;
            explicit KeyLog(QString *o) : out(o) {}
            bool eventFilter(QObject *, QEvent *e) override {
                if (e->type() == QEvent::KeyPress)
                    *out += static_cast<QKeyEvent *>(e)->text();
                return false;
            }
        } keyLog(&typed);
        display->installEventFilter(&keyLog);

        // Started as soon as the tab exists, normally before the session is
        // even connected, so the startup trigger lands inside its WAIT
        QVERIFY(QMetaObject::invokeMethod(&window, "onRunScript", Q_ARG(QString, manualPath)));
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);
        if (stopManualScript) {
            // Stop during the WAIT, once the startup trigger has queued
            QTest::qWait(1000);
            QVERIFY(QMetaObject::invokeMethod(&window, "onStopExecution"));
            QTest::qWait(1500);
            display->removeEventFilter(&keyLog);
            QCOMPARE(typed, QStringLiteral("a"));
            return;
        }
        QTRY_VERIFY_WITH_TIMEOUT(typed.contains('S'), 5000);
        QTest::qWait(300);
        display->removeEventFilter(&keyLog);
        QCOMPARE(typed, QStringLiteral("abS"));
    }
};

QTEST_MAIN(TestStartupScriptWhileRunning)
#include "test_startup_script_while_running.moc"
