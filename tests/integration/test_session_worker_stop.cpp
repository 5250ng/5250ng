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
#include "session/worker.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

using tn5250::client::TN5250Client;
using tn5250::session::Worker;

// File > Disconnect calls Worker::stop() on a live session. The worker must
// report Disconnected itself: the client's signals are cut off before it
// disconnects, so nothing else tells the tab, status bar and MCP registry.
class TestSessionWorkerStop : public QObject {
    Q_OBJECT

  private slots:
    void stopReportsDisconnected() {
        // Fake host: lets TN5250Client finish its handshake (BINARY, then a
        // terminal-type request) and keeps the session open.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        connect(&server, &QTcpServer::newConnection, this, [&server]() {
            while (QTcpSocket *socket = server.nextPendingConnection())
                socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
        });

        ::session::SessionConfig config;
        config.setHostname(QStringLiteral("127.0.0.1"));
        config.setPort(server.serverPort());
        Worker worker;
        worker.setConfig(config);
        QSignalSpy states(&worker, &Worker::stateChanged);
        QSignalSpy disconnected(&worker, &Worker::disconnected);

        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(
            !states.isEmpty()
                && states.last().at(0).value<TN5250Client::ConnectionState>()
                       == TN5250Client::ConnectionState::Connected,
            15000);
        states.clear();

        worker.stop();
        QCOMPARE(states.size(), 1);
        QCOMPARE(states.first().at(0).value<TN5250Client::ConnectionState>(),
                 TN5250Client::ConnectionState::Disconnected);
        QCOMPARE(disconnected.size(), 1);

        // A second stop (nothing left to disconnect) reports nothing more.
        worker.stop();
        QCOMPARE(states.size(), 1);
    }
};

QTEST_MAIN(TestSessionWorkerStop)
#include "test_session_worker_stop.moc"
