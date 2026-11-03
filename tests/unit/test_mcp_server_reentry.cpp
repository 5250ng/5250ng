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

  private:
    static QByteArray request(int id) {
        const QByteArray body =
            QString("{\"jsonrpc\":\"2.0\",\"id\":%1,\"method\":\"ping\"}").arg(id).toUtf8();
        return "POST /mcp HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n"
               "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    }
};

QTEST_MAIN(TestMcpServerReentry)
#include "test_mcp_server_reentry.moc"
