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

#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Session > Reconnect closes the tab and opens a new connection with the
// same settings. It must not use the old session's worker after the worker
// thread has finished (the thread deletes the worker when it finishes).
class TestReconnectTeardown : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void reconnectOpensANewSession() {
        // Fake host: completes the telnet handshake for each connection.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        int connections = 0;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                ++connections;
                socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
            }
        });

        session::SessionConfig config;
        config.setName(QStringLiteral("reconnect-test"));
        config.setHostname(QStringLiteral("127.0.0.1"));
        config.setPort(server.serverPort());

        MainWindow window;
        window.autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);
        auto *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 1);

        for (int i = 0; i < 3; ++i) {
            QVERIFY(QMetaObject::invokeMethod(&window, "onReconnect"));
            QTRY_COMPARE_WITH_TIMEOUT(connections, i + 2, 15000);
            QCOMPARE(tabs->count(), 1);
        }
    }
};

QTEST_MAIN(TestReconnectTeardown)
#include "test_reconnect_teardown.moc"
