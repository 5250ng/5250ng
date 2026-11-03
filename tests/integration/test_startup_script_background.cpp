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
#include "ui/widgets/QConnectionStatusWidget/QConnectionStatusWidget.h"

#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

// A startup script that starts for a tab in the background must not make
// that tab the active session: the window (menus, global status bar)
// would then act on a tab the user cannot see.
class TestStartupScriptBackground : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void backgroundScriptKeepsTheVisibleTabActive() {
        QTcpServer hostA, hostB;
        int connections = 0;
        for (QTcpServer *host : {&hostA, &hostB}) {
            QVERIFY(host->listen(QHostAddress::LocalHost, 0));
            connect(host, &QTcpServer::newConnection, this, [host, &connections]() {
                while (QTcpSocket *socket = host->nextPendingConnection()) {
                    ++connections;
                    socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
                }
            });
        }

        session::SessionConfig a;
        a.setName(QStringLiteral("with-script"));
        a.setHostname(QStringLiteral("127.0.0.1"));
        a.setPort(hostA.serverPort());
        a.setStartupScriptName(QStringLiteral("startup.5250script"));
        a.setStartupScriptSource(QStringLiteral("LOG \"started\"\n"));
        session::SessionConfig b;
        b.setName(QStringLiteral("plain"));
        b.setHostname(QStringLiteral("127.0.0.1"));
        b.setPort(hostB.serverPort());

        MainWindow window;
        window.autoConnect(a);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);
        window.autoConnect(b); // opened second, so it is the visible tab
        QTRY_COMPARE_WITH_TIMEOUT(connections, 2, 15000);
        auto *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 2, 5000);
        tabs->setCurrentIndex(1);

        // The global status bar mirrors the active session
        ui::widgets::QConnectionStatusWidget *global = nullptr;
        for (auto *w : window.findChildren<ui::widgets::QConnectionStatusWidget *>()) {
            if (!w->isHidden())
                global = w;
        }
        QVERIFY(global);
        const QString portB = QString::number(hostB.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(global->statusText().contains(portB), 5000);

        // Tab A's startup script runs about 500 ms after it connected
        QTest::qWait(1500);
        QCOMPARE(tabs->currentIndex(), 1);
        QVERIFY2(global->statusText().contains(portB), qPrintable(global->statusText()));
    }
};

QTEST_MAIN(TestStartupScriptBackground)
#include "test_startup_script_background.moc"
