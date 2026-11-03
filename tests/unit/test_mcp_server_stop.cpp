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
#include <QStandardPaths>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Turning the MCP server off must cut off clients that are already
// connected, not only stop accepting new ones.
class TestMcpServerStop : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void stopDisconnectsKeepAliveClient() {
        mcp::McpServer server;
        QVERIFY(server.start(0));

        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        QVERIFY(!send(client, 1).isEmpty());

        server.stop();
        QVERIFY(!server.isRunning());

        // The server drops the connection; a request on it gets no answer.
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 5000);
        QVERIFY(send(client, 2).isEmpty());
    }

    // A request already sitting in the socket when the server stops is not
    // run either.
    void requestQueuedBeforeStopIsNotRun() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        QVERIFY(!send(client, 1).isEmpty());

        client.write(request(2));
        QVERIFY(client.waitForBytesWritten(5000));
        server.stop(); // before the event loop delivers readyRead
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 5000);
        QVERIFY(client.readAll().isEmpty());
    }

    // Two requests in one write: the second is drained from the parser
    // after the first one's tool call. If the server stops while that tool
    // call runs, the second must not run.
    void pipelinedRequestAfterStopIsNotRun() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        QVERIFY(!send(client, 1).isEmpty());

        // Stop the server while it handles the first of the two requests
        // (as a tool call's nested event loop would), and count the requests
        // the server goes on to handle. The client cannot see this: replies
        // to an aborted socket never arrive.
        int handled = 0;
        connect(&server, &mcp::McpServer::requestLog, this, [&](const QString &entry) {
            if (!entry.contains(QStringLiteral("-> ")))
                return;
            if (++handled == 1)
                server.stop();
        });
        client.write(request(2) + request(3));
        QVERIFY(client.waitForBytesWritten(5000));
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 5000);
        QTest::qWait(200);
        QCOMPARE(handled, 1);
    }

  private:
    static QByteArray request(int id) {
        const QByteArray body =
            QString("{\"jsonrpc\":\"2.0\",\"id\":%1,\"method\":\"ping\"}").arg(id).toUtf8();
        return "POST /mcp HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n"
               "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    }

    // Sends a ping and returns the response, or empty if none arrives.
    static QByteArray send(QTcpSocket &client, int id) {
        if (client.state() != QAbstractSocket::ConnectedState)
            return QByteArray();
        client.write(request(id));
        QByteArray response;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 3000 && !response.contains("\"result\"")) {
            QTest::qWait(20);
            response += client.readAll();
            if (client.state() != QAbstractSocket::ConnectedState)
                break;
        }
        return response.contains("\"result\"") ? response : QByteArray();
    }
};

QTEST_MAIN(TestMcpServerStop)
#include "test_mcp_server_stop.moc"
