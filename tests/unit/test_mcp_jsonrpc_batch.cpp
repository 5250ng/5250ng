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

// Regression tests for issue #427: a JSON-RPC batch, or any other valid
// JSON that is not an object, was answered with -32700 "Parse error: no
// error occurred".
class TestMcpJsonRpcBatch : public QObject {
    Q_OBJECT

    struct Reply {
        int status = 0;
        QByteArray body;
    };

    // POST a raw body and return the HTTP status and body
    static Reply post(mcp::McpServer &server, const QByteArray &body) {
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
                return {reply.mid(9, 3).toInt(), reply.mid(end + 4, len)};
        }
        return {};
    }

    static QJsonObject errorOf(const QByteArray &body) {
        return QJsonDocument::fromJson(body).object().value("error").toObject();
    }

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void batchIsProcessedInOrder() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const Reply r = post(server, R"([{"jsonrpc":"2.0","id":1,"method":"ping"},)"
                                     R"({"jsonrpc":"2.0","method":"notifications/initialized"},)"
                                     R"({"jsonrpc":"2.0","id":"b","method":"tools/list"},)"
                                     R"({"jsonrpc":"2.0","id":3,"method":"no/such"}])");
        QCOMPARE(r.status, 200);
        const QJsonArray responses = QJsonDocument::fromJson(r.body).array();
        // The notification has no response
        QCOMPARE(responses.size(), 3);
        QCOMPARE(responses[0].toObject().value("id").toInt(), 1);
        QVERIFY(responses[0].toObject().contains("result"));
        QCOMPARE(responses[1].toObject().value("id").toString(), QStringLiteral("b"));
        QVERIFY(!responses[1].toObject().value("result").toObject().value("tools").toArray().isEmpty());
        QCOMPARE(responses[2].toObject().value("error").toObject().value("code").toInt(), -32601);
    }

    void nonObjectMembersAreInvalidRequests() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const Reply r = post(server, R"([1,{"jsonrpc":"2.0","id":2,"method":"ping"}])");
        QCOMPARE(r.status, 200);
        const QJsonArray responses = QJsonDocument::fromJson(r.body).array();
        QCOMPARE(responses.size(), 2);
        QCOMPARE(responses[0].toObject().value("error").toObject().value("code").toInt(), -32600);
        QVERIFY(responses[0].toObject().value("id").isNull());
        QCOMPARE(responses[1].toObject().value("id").toInt(), 2);
    }

    // A batch of notifications only gets 202 with no body
    void notificationOnlyBatchIsAccepted() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const Reply r = post(server, R"([{"jsonrpc":"2.0","method":"notifications/initialized"}])");
        QCOMPARE(r.status, 202);
        QVERIFY(r.body.isEmpty());
    }

    void emptyBatchIsInvalidRequest() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const Reply r = post(server, "[]");
        QCOMPARE(errorOf(r.body).value("code").toInt(), -32600);
        QVERIFY(!errorOf(r.body).value("message").toString().contains("no error occurred"));
    }

    // Valid JSON that is not a request is an invalid request, not a
    // parse error
    void scalarBodyIsInvalidRequest_data() {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("number") << QByteArray("1");
        QTest::newRow("string") << QByteArray("\"x\"");
        QTest::newRow("null") << QByteArray("null");
    }
    void scalarBodyIsInvalidRequest() {
        QFETCH(QByteArray, body);
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QCOMPARE(errorOf(post(server, body).body).value("code").toInt(), -32600);
    }

    // More than kMaxBatchSize members is refused as a whole; the limit
    // itself is accepted
    void oversizedBatchIsRefused() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        auto batch = [](int n) {
            QByteArray b = "[";
            for (int i = 0; i < n; ++i)
                b += (i ? ",1" : "1");
            return b + "]";
        };
        const Reply over = post(server, batch(mcp::McpServer::kMaxBatchSize + 1));
        QCOMPARE(errorOf(over.body).value("code").toInt(), -32600);
        QVERIFY(QJsonDocument::fromJson(over.body).isObject());
        const Reply at = post(server, batch(mcp::McpServer::kMaxBatchSize));
        QCOMPARE(QJsonDocument::fromJson(at.body).array().size(), mcp::McpServer::kMaxBatchSize);
    }

    void initializeCannotBeBatched() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const Reply r = post(server, "[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
                                     "\"params\":{}},{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"}]");
        const QJsonArray responses = QJsonDocument::fromJson(r.body).array();
        QCOMPARE(responses.size(), 2);
        QCOMPARE(responses[0].toObject().value("error").toObject().value("code").toInt(), -32600);
        QCOMPARE(responses[0].toObject().value("id").toInt(), 1);
        QVERIFY(responses[1].toObject().contains("result"));
        // As a notification it is dropped without an error entry
        const Reply n = post(server, "[{\"jsonrpc\":\"2.0\",\"method\":\"initialize\"},"
                                     "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"ping\"}]");
        const QJsonArray only = QJsonDocument::fromJson(n.body).array();
        QCOMPARE(only.size(), 1);
        QCOMPARE(only[0].toObject().value("id").toInt(), 3);
    }

    // Stopping the server while a batch runs stops the batch: no further
    // member is handled, as for a pipelined request (#254)
    void stopDuringBatchRunsNoFurtherMember() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        int handled = 0;
        connect(&server, &mcp::McpServer::requestLog, this, [&](const QString &entry) {
            if (!entry.contains(QStringLiteral("] -> ")))
                return;
            if (++handled == 1)
                server.stop();
        });
        post(server, "[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"},"
                     "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"},"
                     "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"ping\"}]");
        QCOMPARE(handled, 1);
    }

    // Invalid JSON is still a parse error, with the parser's reason
    void invalidJsonIsParseError() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        const Reply r = post(server, R"({"jsonrpc":)");
        QCOMPARE(errorOf(r.body).value("code").toInt(), -32700);
        QVERIFY(!errorOf(r.body).value("message").toString().contains("no error occurred"));
    }
};

QTEST_MAIN(TestMcpJsonRpcBatch)
#include "test_mcp_jsonrpc_batch.moc"
