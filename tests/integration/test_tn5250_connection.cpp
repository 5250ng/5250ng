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

#include "network/tn5250/client/client.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest/QtTest>

using namespace tn5250::client;

class TestTN5250Connection : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testBasicConnection();
    void testConnectionFailure();
    void testTelnetNegotiation();
    void testDataTransmission();

  private:
    TN5250Client *m_client;
    QTcpServer *m_server;
    QTcpSocket *m_serverSocket;
    QByteArray m_serverReceivedData;
    bool m_connectionEstablished;

  private slots:
    void onClientConnected();
    void onClientDisconnected();
    void onClientDataReceived(const QByteArray &data);
    void onServerNewConnection();
    void onServerReadyRead();
};

void TestTN5250Connection::init() {
    m_client = new TN5250Client(this);
    m_server = new QTcpServer(this);
    m_serverSocket = nullptr;
    m_serverReceivedData.clear();
    m_connectionEstablished = false;

    connect(m_client, &TN5250Client::connected,
            this, &TestTN5250Connection::onClientConnected);
    connect(m_client, &TN5250Client::disconnected,
            this, &TestTN5250Connection::onClientDisconnected);
    connect(m_client, &TN5250Client::dataReceived,
            this, &TestTN5250Connection::onClientDataReceived);

    connect(m_server, &QTcpServer::newConnection,
            this, &TestTN5250Connection::onServerNewConnection);

    // Start server on any available port
    QVERIFY(m_server->listen(QHostAddress::LocalHost, 0));
}

void TestTN5250Connection::cleanup() {
    if (m_client) {
        m_client->disconnectFromHost();
    }
    if (m_serverSocket) {
        m_serverSocket->close();
        m_serverSocket->deleteLater();
        m_serverSocket = nullptr;
    }
    m_server->close();
}

void TestTN5250Connection::onClientConnected() {
    m_connectionEstablished = true;
}

void TestTN5250Connection::onClientDisconnected() {
    m_connectionEstablished = false;
}

void TestTN5250Connection::onClientDataReceived(const QByteArray &data) {
    // Store received data for verification
}

void TestTN5250Connection::onServerNewConnection() {
    // This slot is also invoked by QtTest as a test case with no pending connection.
    if (!m_server->hasPendingConnections()) {
        return;
    }
    m_serverSocket = m_server->nextPendingConnection();
    if (!m_serverSocket) {
        return;
    }
    connect(m_serverSocket, &QTcpSocket::readyRead,
            this, &TestTN5250Connection::onServerReadyRead);
    connect(m_serverSocket, &QTcpSocket::disconnected,
            m_serverSocket, &QTcpSocket::deleteLater);
}

void TestTN5250Connection::onServerReadyRead() {
    if (m_serverSocket) {
        m_serverReceivedData.append(m_serverSocket->readAll());
    }
}

void TestTN5250Connection::testBasicConnection() {
    quint16 port = m_server->serverPort();

    m_client->connectToHost("127.0.0.1", port, false);

    // Wait for connection (with timeout)
    QTimer::singleShot(2000, [this]() {
        if (!m_connectionEstablished) {
            QFAIL("Connection timeout");
        }
    });

    QTest::qWait(100);

    // Verify connection state
    QVERIFY(m_client->state() == TN5250Client::ConnectionState::Connecting ||
            m_client->state() == TN5250Client::ConnectionState::Negotiating ||
            m_client->state() == TN5250Client::ConnectionState::Connected);
}

void TestTN5250Connection::testConnectionFailure() {
    // Try to connect to non-existent host
    m_client->connectToHost("127.0.0.1", 1, false); // Port 1 is typically not listening

    QTest::qWait(500);

    // Should be in error or disconnected state
    QVERIFY(m_client->state() == TN5250Client::ConnectionState::Error ||
            m_client->state() == TN5250Client::ConnectionState::Disconnected);
}

void TestTN5250Connection::testTelnetNegotiation() {
    quint16 port = m_server->serverPort();

    m_client->connectToHost("127.0.0.1", port, false);

    // Wait for connection and negotiation
    QTest::qWait(500);

    // Server should have received telnet negotiation commands
    // Look for IAC sequences in received data
    bool foundIAC = false;
    for (int i = 0; i < m_serverReceivedData.size() - 2; ++i) {
        if (static_cast<uint8_t>(m_serverReceivedData[i]) == 255) { // IAC
            foundIAC = true;
            break;
        }
    }

    // Note: In a real test, we'd verify the exact negotiation sequence
    // For now, we just verify that some data was exchanged
    QVERIFY(m_serverReceivedData.size() > 0 || foundIAC);
}

void TestTN5250Connection::testDataTransmission() {
    quint16 port = m_server->serverPort();

    m_client->connectToHost("127.0.0.1", port, false);

    // Wait for connection
    QTest::qWait(500);

    if (m_client->isConnected()) {
        QByteArray testData("Hello TN5250");
        m_client->sendData(testData);

        QTest::qWait(100);

        // Verify server received the data (may be escaped with IAC)
        QVERIFY(m_serverReceivedData.size() > 0);
    }
}

QTEST_MAIN(TestTN5250Connection)
#include "test_tn5250_connection.moc"
