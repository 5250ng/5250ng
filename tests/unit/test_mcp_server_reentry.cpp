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
#include <QApplication>
#include <QEventLoop>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Regression test for issue #254: a request that arrived on the same
// connection while a tool call ran a nested event loop made the server
// dispatch the in-flight request a second time.
class TestMcpServerReentry : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void requestDuringNestedLoopRunsOnceInOrder() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));

        // While the first request is handled, send the second one and run a
        // nested event loop, as run_5250script and wait_for_text do
        int handled = 0;
        connect(&server, &mcp::McpServer::requestLog, this, [&](const QString &entry) {
            if (!entry.contains(QStringLiteral("-> ")))
                return;
            if (++handled != 1)
                return;
            client.write(request(2));
            client.flush();
            QEventLoop loop;
            QTimer::singleShot(400, &loop, &QEventLoop::quit);
            loop.exec();
        });
        client.write(request(1));
        QVERIFY(client.waitForBytesWritten(5000));

        QByteArray responses;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 5000 && responses.count("\"jsonrpc\"") < 2) {
            QTest::qWait(20);
            responses += client.readAll();
        }
        QTest::qWait(300);
        responses += client.readAll();

        QStringList ids;
        const QRegularExpression idRe(QStringLiteral("\"id\":(\\d+)"));
        for (auto it = idRe.globalMatch(QString::fromUtf8(responses)); it.hasNext();)
            ids << it.next().captured(1);
        QCOMPARE(handled, 2);
        QCOMPARE(ids, (QStringList{QStringLiteral("1"), QStringLiteral("2")}));
    }

    // Issue #430: responses said "Connection: close" but left the socket
    // open. Normal responses are now persistent (no close header) and a
    // second request on the same connection is answered.
    void responsesArePersistent() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        client.write(request(1));
        QByteArray first = readResponses(client, 1);
        QVERIFY2(first.startsWith("HTTP/1.1 200"), first.constData());
        QVERIFY2(!first.contains("Connection: close"), first.constData());
        client.write(request(2));
        const QByteArray second = readResponses(client, 1);
        QVERIFY2(second.contains("\"id\":2"), second.constData());
        QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    }

    // A response that does close the connection says so, and the client
    // sees EOF right after it
    void closingResponseSaysClose() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        client.write("POST /mcp HTTP/1.1\r\nHost: evil.example\r\nContent-Type: application/json\r\n"
                     "Content-Length: 2\r\n\r\n{}");
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 3000);
        const QByteArray reply = client.readAll();
        QVERIFY2(reply.startsWith("HTTP/1.1 403") && reply.contains("Connection: close"), reply.constData());
    }

    // A client's "Connection: close" (RFC 9112 9.6) and HTTP/1.0 without
    // keep-alive (9.3): the response says close, the connection is closed
    // after it, and a request pipelined behind it is not run
    void clientCloseIsHonoured_data() {
        QTest::addColumn<QByteArray>("head");
        QTest::newRow("Connection: close")
            << QByteArray("POST /mcp HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-Alive, Close\r\n");
        QTest::newRow("HTTP/1.0") << QByteArray("POST /mcp HTTP/1.0\r\nHost: localhost\r\n");
    }
    void clientCloseIsHonoured() {
        QFETCH(QByteArray, head);
        mcp::McpServer server;
        QVERIFY(server.start(0));
        int handled = 0;
        connect(&server, &mcp::McpServer::requestLog, this, [&handled](const QString &entry) {
            if (entry.contains(QStringLiteral("-> ")))
                ++handled;
        });
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        const QByteArray body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}";
        client.write(head + "Content-Type: application/json\r\nContent-Length: "
                     + QByteArray::number(body.size()) + "\r\n\r\n" + body + request(2));
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 3000);
        const QByteArray reply = client.readAll();
        QVERIFY2(reply.startsWith("HTTP/1.1 200") && reply.contains("Connection: close"), reply.constData());
        QVERIFY2(reply.contains("\"id\":1") && !reply.contains("\"id\":2"), reply.constData());
        QCOMPARE(handled, 1);
    }

    // HTTP/1.0 with keep-alive stays open
    void http10KeepAliveStaysOpen() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        const QByteArray body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}";
        client.write("POST /mcp HTTP/1.0\r\nHost: localhost\r\nConnection: keep-alive\r\n"
                     "Content-Type: application/json\r\nContent-Length: "
                     + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        const QByteArray reply = readResponses(client, 1);
        QVERIFY2(reply.contains("\"id\":1") && !reply.contains("Connection: close"), reply.constData());
        QTest::qWait(200);
        QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    }

    // A parse error closes the connection and says so
    void parseErrorSaysClose() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        client.write("POST /mcp HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n");
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 3000);
        const QByteArray reply = client.readAll();
        QVERIFY2(reply.startsWith("HTTP/1.1 400") && reply.contains("Connection: close"), reply.constData());
    }

    void idleTimeoutDefaults() {
        mcp::McpServer server;
        QCOMPARE(server.idleTimeoutMs(), mcp::McpServer::kDefaultIdleTimeoutMs);
        server.setIdleTimeoutMs(0);
        QCOMPARE(server.idleTimeoutMs(), mcp::McpServer::kDefaultIdleTimeoutMs);
        server.setIdleTimeoutMs(-1);
        QVERIFY(server.idleTimeoutMs() < 0);
        server.setIdleTimeoutMs(250);
        QCOMPARE(server.idleTimeoutMs(), 250);
    }

    // A silent connection and an idle one after a response are closed; a
    // request whose handling outlasts the timeout is still answered
    void idleConnectionsAreClosed() {
        mcp::McpServer server;
        server.setIdleTimeoutMs(300);
        QVERIFY(server.start(0));

        QTcpSocket silent;
        silent.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(silent.waitForConnected(5000));
        QTRY_COMPARE_WITH_TIMEOUT(silent.state(), QAbstractSocket::UnconnectedState, 3000);

        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        // Hold the request in a nested loop for longer than the timeout,
        // as a running script does
        connect(&server, &mcp::McpServer::requestLog, this, [](const QString &entry) {
            if (!entry.contains(QStringLiteral("-> ")))
                return;
            QEventLoop loop;
            QTimer::singleShot(800, &loop, &QEventLoop::quit);
            loop.exec();
        });
        client.write(request(5));
        const QByteArray reply = readResponses(client, 1);
        QVERIFY2(reply.contains("\"id\":5"), reply.constData());
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 3000);
    }

    // A negative timeout keeps a quiet connection open
    void disabledIdleTimeoutKeepsConnection() {
        mcp::McpServer server;
        server.setIdleTimeoutMs(-1);
        QVERIFY(server.start(0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        QTest::qWait(600);
        QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    }

  private:
    // Read until `count` complete responses (by Content-Length) arrived
    static QByteArray readResponses(QTcpSocket &client, int count) {
        QByteArray data;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 5000) {
            QTest::qWait(20);
            data += client.readAll();
            int pos = 0, complete = 0;
            while (true) {
                const int end = data.indexOf("\r\n\r\n", pos);
                if (end < 0) break;
                const int lenPos = data.indexOf("Content-Length: ", pos);
                const int len = (lenPos >= 0 && lenPos < end)
                    ? data.mid(lenPos + 16, data.indexOf("\r\n", lenPos) - lenPos - 16).toInt() : 0;
                if (data.size() < end + 4 + len) break;
                pos = end + 4 + len;
                ++complete;
            }
            if (complete >= count)
                return data;
        }
        return data;
    }

    static QByteArray request(int id) {
        const QByteArray body =
            QString("{\"jsonrpc\":\"2.0\",\"id\":%1,\"method\":\"ping\"}").arg(id).toUtf8();
        return "POST /mcp HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n"
               "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    }
};

QTEST_MAIN(TestMcpServerReentry)
#include "test_mcp_server_reentry.moc"
