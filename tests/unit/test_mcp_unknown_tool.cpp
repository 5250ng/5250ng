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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Regression tests for issue #428: tools/call with an unknown or missing
// tool name returned a successful result with isError instead of a
// JSON-RPC -32602 error.
class TestMcpUnknownTool : public QObject {
    Q_OBJECT

    // POST one JSON-RPC request and return the parsed response body
    static QJsonObject call(mcp::McpServer &server, const QJsonObject &request) {
        const QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        if (!client.waitForConnected(5000))
            return {};
        client.write("POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
                     "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        QByteArray reply;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 3000) {
            QTest::qWait(20);
            reply += client.readAll();
            const int end = reply.indexOf("\r\n\r\n");
            if (end < 0)
                continue;
            const int lenPos = reply.indexOf("Content-Length: ");
            const int len = reply.mid(lenPos + 16, reply.indexOf("\r\n", lenPos) - lenPos - 16).toInt();
            if (reply.size() >= end + 4 + len)
                return QJsonDocument::fromJson(reply.mid(end + 4, len)).object();
        }
        return {};
    }

    static QJsonObject toolsCall(const QJsonValue &name) {
        QJsonObject params{{"arguments", QJsonObject()}};
        if (!name.isUndefined())
            params["name"] = name;
        return QJsonObject{{"jsonrpc", "2.0"}, {"id", 9}, {"method", "tools/call"}, {"params", params}};
    }

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void unknownToolIsInvalidParams_data() {
        QTest::addColumn<QJsonValue>("name");
        QTest::addColumn<QString>("message");
        QTest::newRow("unknown") << QJsonValue("nope") << "Unknown tool: nope";
        QTest::newRow("missing") << QJsonValue(QJsonValue::Undefined) << "No tool name provided.";
        QTest::newRow("empty") << QJsonValue("") << "No tool name provided.";
        QTest::newRow("not a string") << QJsonValue(42) << "No tool name provided.";
        QTest::newRow("removed connect") << QJsonValue("connect") << "replaced by 'create_session'";
        QTest::newRow("agent-only tool") << QJsonValue("generate_5250script") << "not available over MCP";
    }
    void unknownToolIsInvalidParams() {
        QFETCH(QJsonValue, name);
        QFETCH(QString, message);
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const QJsonObject r = call(server, toolsCall(name));
        QVERIFY2(!r.contains("result"), QJsonDocument(r).toJson().constData());
        const QJsonObject error = r.value("error").toObject();
        QCOMPARE(error.value("code").toInt(), -32602);
        QVERIFY2(error.value("message").toString().contains(message),
                 qPrintable(error.value("message").toString()));
        QCOMPARE(r.value("id").toInt(), 9);
    }

    // Every tool tools/list advertises is still dispatched as a tool call;
    // errors while running a known tool stay results with isError
    void knownToolsStillRun() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const QJsonObject list = call(server, QJsonObject{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}});
        const QJsonArray tools = list.value("result").toObject().value("tools").toArray();
        QVERIFY(tools.size() > 10);
        const QJsonObject r = call(server, toolsCall("list_sessions"));
        QVERIFY2(r.contains("result"), QJsonDocument(r).toJson().constData());
        // A known session tool without its session gets a tool error result
        const QJsonObject e = call(server, toolsCall("read_screen"));
        QVERIFY2(e.contains("result") && e.value("result").toObject().value("isError").toBool(),
                 QJsonDocument(e).toJson().constData());
    }
};

QTEST_MAIN(TestMcpUnknownTool)
#include "test_mcp_unknown_tool.moc"
