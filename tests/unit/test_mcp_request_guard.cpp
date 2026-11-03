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
#include "mcp/mcp_request_guard.h"
#include <QApplication>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Web pages must not be able to drive the localhost MCP server, neither by
// a cross-site POST nor after DNS rebinding; MCP clients keep working.
class TestMcpRequestGuard : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void hostHeader_data() {
        QTest::addColumn<QString>("host");
        QTest::addColumn<bool>("allowed");
        QTest::newRow("localhost:port") << "localhost:9250" << true;
        QTest::newRow("127.0.0.1") << "127.0.0.1" << true;
        QTest::newRow("[::1]:9250") << "[::1]:9250" << true;
        QTest::newRow("LOCALHOST") << "LOCALHOST:9250" << true;
        QTest::newRow("absent") << "" << true;
        QTest::newRow("rebound name") << "rebind.attacker.example:9250" << false;
        QTest::newRow("localhost suffix") << "localhost.attacker.example" << false;
        QTest::newRow("other address") << "192.168.1.10:9250" << false;
        QTest::newRow("userinfo") << "localhost@evil.example" << false;
    }
    void hostHeader() {
        QFETCH(QString, host);
        QFETCH(bool, allowed);
        QCOMPARE(mcp::isAllowedHostHeader(host), allowed);
    }

    void origin_data() {
        QTest::addColumn<QString>("origin");
        QTest::addColumn<bool>("allowed");
        QTest::newRow("absent") << "" << true;
        QTest::newRow("loopback") << "http://127.0.0.1:6274" << true;
        QTest::newRow("localhost") << "http://localhost:3000" << true;
        QTest::newRow("foreign") << "http://evil.example" << false;
        QTest::newRow("null") << "null" << false;
        QTest::newRow("file") << "file://" << false;
    }
    void origin() {
        QFETCH(QString, origin);
        QFETCH(bool, allowed);
        QCOMPARE(mcp::isAllowedOrigin(origin), allowed);
    }

    void contentType() {
        QVERIFY(mcp::isJsonContentType("application/json"));
        QVERIFY(mcp::isJsonContentType("Application/JSON; charset=utf-8"));
        QVERIFY(!mcp::isJsonContentType("text/plain"));
        QVERIFY(!mcp::isJsonContentType("application/x-www-form-urlencoded"));
        QVERIFY(!mcp::isJsonContentType(""));
    }

    // The issue's request: a page posting text/plain with a foreign Origin
    // and a rebound Host gets no tool result, each check alone is enough,
    // and a normal MCP client request still works.
    void serverRefusesBrowserRequests() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const QByteArray call =
            R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"list_sessions","arguments":{}}})";
        const QByteArray local = "Host: 127.0.0.1\r\n";

        QByteArray r = post(server, "Origin: http://evil.example\r\nHost: rebind.attacker.example\r\n"
                                    "Content-Type: text/plain\r\n", call);
        QVERIFY2(r.startsWith("HTTP/1.1 403") && !r.contains("\"result\""), r.constData());
        r = post(server, "Host: rebind.attacker.example\r\nContent-Type: application/json\r\n", call);
        QVERIFY2(r.startsWith("HTTP/1.1 403"), r.constData());
        r = post(server, local + "Origin: http://evil.example\r\nContent-Type: application/json\r\n", call);
        QVERIFY2(r.startsWith("HTTP/1.1 403"), r.constData());
        r = post(server, local + "Content-Type: text/plain\r\n", call);
        QVERIFY2(r.startsWith("HTTP/1.1 415"), r.constData());

        r = post(server, local + "Content-Type: application/json\r\n", call);
        QVERIFY2(r.startsWith("HTTP/1.1 200") && r.contains("\"result\""), r.constData());
    }

  private:
    static QByteArray post(mcp::McpServer &server, const QByteArray &headers, const QByteArray &body) {
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        if (!client.waitForConnected(5000))
            return QByteArray();
        client.write("POST /mcp HTTP/1.1\r\n" + headers + "Content-Length: "
                     + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        QByteArray reply;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 3000) {
            QTest::qWait(20);
            reply += client.readAll();
            const int end = reply.indexOf("\r\n\r\n");
            if (end >= 0) {
                const int lenPos = reply.indexOf("Content-Length: ");
                const int len = lenPos >= 0 ? reply.mid(lenPos + 16, reply.indexOf("\r\n", lenPos) - lenPos - 16).toInt() : 0;
                if (reply.size() >= end + 4 + len)
                    break;
            }
        }
        return reply;
    }
};

QTEST_MAIN(TestMcpRequestGuard)
#include "test_mcp_request_guard.moc"
