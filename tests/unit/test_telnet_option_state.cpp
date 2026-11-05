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

#include "network/tn5250_qt/client/client.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

using tn5250::client::TN5250Client;

// Regression tests for issue #272 (items 1 and 2), against a local telnet
// server: the client re-acknowledged options already in effect, and
// answered an unsolicited NEW-ENVIRON IS as if the server had asked.
class TestTelnetOptionState : public QObject {
    Q_OBJECT

    static constexpr char IAC = '\xFF', WILL = '\xFB', WONT = '\xFC', DO = '\xFD', DONT = '\xFE',
                          SB = '\xFA', SE = '\xF0';
    static constexpr char BINARY = '\x00', EOR = '\x19', NEW_ENVIRON = '\x27';

    QTcpServer m_server;
    QTcpSocket *m_peer = nullptr;
    std::unique_ptr<TN5250Client> m_client;

    // Everything the client sends within `ms`
    QByteArray readFor(int ms) {
        QByteArray got;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms) {
            QTest::qWait(20);
            got += m_peer->readAll();
        }
        return got;
    }

    // Waits up to 5 s for `n` bytes, so a slow runner cannot push a late
    // reply into the next check's window
    QByteArray readBytes(int n) {
        QByteArray got;
        QElapsedTimer t;
        t.start();
        while (got.size() < n && t.elapsed() < 5000) {
            QTest::qWait(20);
            got += m_peer->readAll();
        }
        return got;
    }

    // Waits up to 5 s for a reply containing `needle`
    QByteArray readUntil(const QByteArray &needle) {
        QByteArray got;
        QElapsedTimer t;
        t.start();
        while (!got.contains(needle) && t.elapsed() < 5000) {
            QTest::qWait(20);
            got += m_peer->readAll();
        }
        got += readFor(100);
        return got;
    }

    static QByteArray cmd(char c, char opt) { return QByteArray() + IAC + c + opt; }

  private slots:
    void init() {
        QVERIFY(m_server.listen(QHostAddress::LocalHost, 0));
        m_client = std::make_unique<TN5250Client>();
        m_client->setDeviceName("QPADEV9999");
        m_client->connectToHost("127.0.0.1", m_server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(m_server.hasPendingConnections(), 3000);
        m_peer = m_server.nextPendingConnection();
        // The client opens with its own requests
        QByteArray opening = readBytes(12);
        opening += readFor(200);
        QVERIFY2(opening.contains(cmd(DO, BINARY)) && opening.contains(cmd(WILL, BINARY)) &&
                     opening.contains(cmd(DO, EOR)) && opening.contains(cmd(WILL, EOR)),
                 opening.toHex().constData());
    }
    void cleanup() {
        m_client.reset();
        delete m_peer;
        m_peer = nullptr;
        m_server.close();
    }

    // The server's WILL/DO is acknowledged once, as the IBM i server
    // expects even after the client's own opening requests, and a repeat
    // once the option is on is not acknowledged again (RFC 854)
    void repeatsAreNotAcknowledged() {
        m_peer->write(cmd(WILL, BINARY) + cmd(DO, BINARY) + cmd(WILL, EOR) + cmd(DO, EOR));
        QCOMPARE(readBytes(12).toHex(),
                 (cmd(DO, BINARY) + cmd(WILL, BINARY) + cmd(DO, EOR) + cmd(WILL, EOR)).toHex());
        m_peer->write(cmd(WILL, BINARY) + cmd(DO, EOR) + cmd(WILL, EOR) + cmd(DO, BINARY));
        QCOMPARE(readFor(300).toHex(), QByteArray());
    }

    // A server-initiated request is still agreed to, once
    void serverRequestIsAgreedOnce() {
        m_peer->write(cmd(DO, NEW_ENVIRON));
        QCOMPARE(readBytes(3).toHex(), cmd(WILL, NEW_ENVIRON).toHex());
        m_peer->write(cmd(DO, NEW_ENVIRON));
        QCOMPARE(readFor(300).toHex(), QByteArray());
    }

    // A SEND is answered even before DO NEW-ENVIRON (as tn5250j does)
    void sendWithoutDoIsAnswered() {
        m_peer->write(QByteArray() + IAC + SB + NEW_ENVIRON + '\x01' + IAC + SE);
        const QByteArray reply = readUntil("QPADEV9999");
        QVERIFY2(reply.contains("DEVNAME") && reply.contains("QPADEV9999"), reply.toHex().constData());
    }

    // The server switching its side off is acknowledged once
    void serverWontIsAcknowledged() {
        m_peer->write(cmd(WILL, BINARY));
        QCOMPARE(readBytes(3).toHex(), cmd(DO, BINARY).toHex());
        m_peer->write(cmd(WONT, BINARY));
        QCOMPARE(readBytes(3).toHex(), cmd(DONT, BINARY).toHex());
        m_peer->write(cmd(WONT, BINARY));
        QCOMPARE(readFor(300).toHex(), QByteArray());
    }

    // An option switched off is acknowledged, and can be agreed again
    void disableIsAcknowledged() {
        m_peer->write(cmd(DO, NEW_ENVIRON));
        QCOMPARE(readBytes(3).toHex(), cmd(WILL, NEW_ENVIRON).toHex());
        m_peer->write(cmd(DONT, NEW_ENVIRON));
        QCOMPARE(readBytes(3).toHex(), cmd(WONT, NEW_ENVIRON).toHex());
        m_peer->write(cmd(DONT, NEW_ENVIRON));
        QCOMPARE(readFor(300).toHex(), QByteArray());
    }

    // An IS the server sends on its own is not answered, and does not use
    // up the device name before the real SEND
    void unsolicitedIsGetsNoReply() {
        m_peer->write(QByteArray() + IAC + SB + NEW_ENVIRON + '\x00' + IAC + SE);
        QCOMPARE(readFor(300).toHex(), QByteArray());
        m_peer->write(cmd(DO, NEW_ENVIRON));
        QCOMPARE(readBytes(3).toHex(), cmd(WILL, NEW_ENVIRON).toHex());
        m_peer->write(QByteArray() + IAC + SB + NEW_ENVIRON + '\x00' + IAC + SE);
        QCOMPARE(readFor(300).toHex(), QByteArray());
        m_peer->write(QByteArray() + IAC + SB + NEW_ENVIRON + '\x01' + IAC + SE);
        const QByteArray reply = readUntil("QPADEV9999");
        QVERIFY2(reply.contains("DEVNAME") && reply.contains("QPADEV9999"), reply.toHex().constData());
    }
};

QTEST_MAIN(TestTelnetOptionState)
#include "test_telnet_option_state.moc"
