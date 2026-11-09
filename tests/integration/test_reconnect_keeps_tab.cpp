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

#include "mcp/McpServer.h"
#include "session/config.h"
#include "session/manager.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Issue #470: Reconnect closed the tab and opened a new one at the end of
// the tab bar, under the profile's name instead of the tab's label.
class TestReconnectKeepsTab : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void reconnectKeepsPositionAndLabel() {
        QTcpServer host;
        QVERIFY(host.listen(QHostAddress::LocalHost, 0));
        int connections = 0;
        connect(&host, &QTcpServer::newConnection, this, [&]() {
            while (QTcpSocket *socket = host.nextPendingConnection()) {
                ++connections;
                socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
            }
        });

        auto config = [&](const QString &name) {
            session::SessionConfig c;
            c.setName(name);
            c.setHostname(QStringLiteral("127.0.0.1"));
            c.setPort(host.serverPort());
            session::SessionManager().saveSession(c);
            return c;
        };
        MainWindow *window = new MainWindow();
        window->autoConnect(config(QStringLiteral("reconnect-first")));
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);
        window->autoConnect(config(QStringLiteral("reconnect-second")));
        QTRY_COMPARE_WITH_TIMEOUT(connections, 2, 15000);

        auto *tabs = window->findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        tabs->setTabText(0, QStringLiteral("Renamed")); // as Rename does
        QWidget *second = tabs->widget(1);
        tabs->setCurrentIndex(0);

        QVERIFY(QMetaObject::invokeMethod(window, "onReconnect"));
        QTRY_COMPARE_WITH_TIMEOUT(connections, 3, 15000);

        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->tabText(0), QStringLiteral("Renamed"));
        QCOMPARE(tabs->widget(1), second); // the other tab did not move
        QCOMPARE(tabs->currentIndex(), 0);

        QTest::qWait(200); // let the closed tab finish its deferred teardown
        delete window;
        session::SessionManager().deleteSession(QStringLiteral("reconnect-first"));
        session::SessionManager().deleteSession(QStringLiteral("reconnect-second"));
    }

    // An MCP client's tab keeps its session id and stays current when it is
    // reconnected as the last tab: the rule that opens MCP tabs in the
    // background (#413) must not apply to it
    void reconnectedMcpTabStaysCurrent() {
        QTcpServer host;
        QVERIFY(host.listen(QHostAddress::LocalHost, 0));
        int connections = 0;
        connect(&host, &QTcpServer::newConnection, this, [&]() {
            while (QTcpSocket *socket = host.nextPendingConnection()) {
                ++connections;
                socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
            }
        });
        session::SessionConfig config;
        config.setName(QStringLiteral("reconnect-user"));
        config.setHostname(QStringLiteral("127.0.0.1"));
        config.setPort(host.serverPort());

        MainWindow *window = new MainWindow();
        window->autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);
        auto *mcp = window->findChild<mcp::McpServer *>();
        QVERIFY(mcp);
        config.setName(QStringLiteral("reconnect-mcp"));
        emit mcp->createSessionRequested(QStringLiteral("mcp-reconnect-id"), config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 2, 15000);

        auto *tabs = window->findChild<QTabWidget *>();
        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->tabToolTip(1).contains(QStringLiteral("mcp-reconnect-id")));
        tabs->setCurrentIndex(1);

        QVERIFY(QMetaObject::invokeMethod(window, "onReconnect"));
        QTRY_COMPARE_WITH_TIMEOUT(connections, 3, 15000);

        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->currentIndex(), 1);
        QVERIFY(tabs->tabToolTip(1).contains(QStringLiteral("mcp-reconnect-id")));

        QTest::qWait(200);
        delete window;
    }
};

QTEST_MAIN(TestReconnectKeepsTab)
#include "test_reconnect_keeps_tab.moc"
