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

namespace {

// Telnet bytes that let TN5250Client finish its handshake: the host agrees
// to BINARY, then asks for the terminal type, which the client answers.
const QByteArray kHostHandshake = QByteArray::fromHex(
    "fffb00"     // IAC WILL BINARY
    "fffd00"     // IAC DO BINARY
    "fffa1801fff0"); // IAC SB TERMINAL-TYPE SEND IAC SE

// Fake host: completes the handshake for every connection and, while
// dropConnections is set, closes each session once it is established.
class FakeHost : public QObject {
  public:
    QTcpServer server;
    int connections = 0;
    bool dropConnections = true;

    FakeHost() {
        connect(&server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                ++connections;
                socket->write(kHostHandshake);
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    const QByteArray data = socket->readAll();
                    // The terminal-type reply means the client is now Connected
                    if (dropConnections && data.contains(QByteArray::fromHex("fffa1800")))
                        QTimer::singleShot(50, socket, [socket]() { socket->disconnectFromHost(); });
                });
            }
        });
    }
};

// Generous waits: on Windows a refused connection to a closed localhost
// port takes about two seconds (the SYN is retried), and CI runners are slow.
constexpr int kWaitMs = 15000;
constexpr int kRefusedAttemptsWaitMs = 30000;

::session::SessionConfig makeConfig(quint16 port) {
    ::session::SessionConfig config;
    config.setHostname(QStringLiteral("127.0.0.1"));
    config.setPort(port);
    config.setAutoReconnect(true);
    config.setAutoReconnectDelaySeconds(1);
    config.setAutoReconnectMaxAttempts(2);
    return config;
}

} // namespace

class TestSessionWorkerReconnect : public QObject {
    Q_OBJECT

  private slots:
    void testReconnectsAfterDrop();
    void testGivesUpAfterMaxAttempts();
    void testNoReconnectWhenDisabled();
    void testNoReconnectAfterStop();
    void testStopCancelsPendingReconnect();
};

void TestSessionWorkerReconnect::testReconnectsAfterDrop() {
    FakeHost host;
    QVERIFY(host.server.listen(QHostAddress::LocalHost, 0));

    Worker worker;
    worker.setConfig(makeConfig(host.server.serverPort()));
    QSignalSpy scheduled(&worker, &Worker::reconnectScheduled);
    QSignalSpy errors(&worker, &Worker::errorOccurred);
    int connectedCount = 0;
    connect(&worker, &Worker::stateChanged, this, [&](TN5250Client::ConnectionState s) {
        if (s == TN5250Client::ConnectionState::Connected) {
            ++connectedCount;
            host.dropConnections = connectedCount < 2; // keep the second session up
        }
    });

    worker.start();
    QTRY_COMPARE_WITH_TIMEOUT(scheduled.size(), 1, kWaitMs);
    QCOMPARE(scheduled.first().at(0).toInt(), 1); // attempt
    QCOMPARE(scheduled.first().at(1).toInt(), 2); // max attempts
    QCOMPARE(scheduled.first().at(2).toInt(), 1); // delay
    QTRY_COMPARE_WITH_TIMEOUT(connectedCount, 2, kWaitMs);
    QCOMPARE(host.connections, 2);
    QVERIFY2(errors.isEmpty(), "a dropped session being reconnected must not report an error");
    worker.stop();
}

void TestSessionWorkerReconnect::testGivesUpAfterMaxAttempts() {
    FakeHost host;
    QVERIFY(host.server.listen(QHostAddress::LocalHost, 0));
    const quint16 port = host.server.serverPort();

    Worker worker;
    worker.setConfig(makeConfig(port));
    QSignalSpy scheduled(&worker, &Worker::reconnectScheduled);
    QSignalSpy errors(&worker, &Worker::errorOccurred);

    worker.start();
    QTRY_COMPARE_WITH_TIMEOUT(scheduled.size(), 1, kWaitMs);
    host.server.close(); // later attempts are refused

    // Two attempts are allowed; after the second refusal the error surfaces
    QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, kRefusedAttemptsWaitMs);
    QCOMPARE(scheduled.size(), 2);
    worker.stop();
}

void TestSessionWorkerReconnect::testNoReconnectWhenDisabled() {
    FakeHost host;
    QVERIFY(host.server.listen(QHostAddress::LocalHost, 0));

    Worker worker;
    ::session::SessionConfig config = makeConfig(host.server.serverPort());
    config.setAutoReconnect(false);
    worker.setConfig(config);
    QSignalSpy scheduled(&worker, &Worker::reconnectScheduled);
    QSignalSpy disconnected(&worker, &Worker::disconnected);

    worker.start();
    QTRY_VERIFY_WITH_TIMEOUT(!disconnected.isEmpty(), kWaitMs);
    QTest::qWait(1500);
    QCOMPARE(scheduled.size(), 0);
    QCOMPARE(host.connections, 1);
    worker.stop();
}

void TestSessionWorkerReconnect::testNoReconnectAfterStop() {
    FakeHost host;
    host.dropConnections = false;
    QVERIFY(host.server.listen(QHostAddress::LocalHost, 0));

    Worker worker;
    worker.setConfig(makeConfig(host.server.serverPort()));
    QSignalSpy scheduled(&worker, &Worker::reconnectScheduled);
    bool connected = false;
    connect(&worker, &Worker::stateChanged, this, [&](TN5250Client::ConnectionState s) {
        if (s == TN5250Client::ConnectionState::Connected) connected = true;
    });

    worker.start();
    QTRY_VERIFY_WITH_TIMEOUT(connected, kWaitMs);
    worker.stop(); // user-initiated disconnect
    QTest::qWait(1500);
    QCOMPARE(scheduled.size(), 0);
    QCOMPARE(host.connections, 1);
}

void TestSessionWorkerReconnect::testStopCancelsPendingReconnect() {
    FakeHost host;
    QVERIFY(host.server.listen(QHostAddress::LocalHost, 0));

    Worker worker;
    ::session::SessionConfig config = makeConfig(host.server.serverPort());
    config.setAutoReconnectDelaySeconds(2);
    worker.setConfig(config);
    QSignalSpy scheduled(&worker, &Worker::reconnectScheduled);
    QSignalSpy states(&worker, &Worker::stateChanged);

    worker.start();
    QTRY_COMPARE_WITH_TIMEOUT(scheduled.size(), 1, kWaitMs);
    states.clear();
    worker.stop();
    QCOMPARE(states.size(), 1);
    QCOMPARE(states.first().at(0).value<TN5250Client::ConnectionState>(),
             TN5250Client::ConnectionState::Disconnected);
    QTest::qWait(2500);
    QCOMPARE(host.connections, 1);
}

QTEST_MAIN(TestSessionWorkerReconnect)
#include "test_session_worker_reconnect.moc"
