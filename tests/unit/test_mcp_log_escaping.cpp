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

#include "logger/logger.h"
#include "mcp/McpServer.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Regression test for issue #435: the request log line carried the
// client's raw method and tool name, so control characters reached the
// MCP log viewer.
class TestMcpLogEscaping : public QObject {
    Q_OBJECT

    static void post(mcp::McpServer &server, const QByteArray &body) {
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        client.write("POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
                     "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        QVERIFY(client.waitForBytesWritten(5000));
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0 || client.waitForReadyRead(50), 3000);
    }

    static QString requestLine(const QSignalSpy &spy) {
        for (const QList<QVariant> &args : spy) {
            const QString entry = args.at(0).toString();
            if (entry.contains(QStringLiteral("] -> ")))
                return entry;
        }
        return QString();
    }

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void toolNameIsEscaped() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QSignalSpy spy(&server, &mcp::McpServer::requestLog);
        post(server, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":"
                     "{\"name\":\"x\\u001b[31mfake\\n[00:00:00] <- OK\",\"arguments\":{}}}");
        const QString line = requestLine(spy);
        QVERIFY2(!line.isEmpty(), "no request log line");
        QVERIFY2(!line.contains(QChar(0x1b)) && !line.contains('\n'), qPrintable(line));
        QVERIFY2(line.contains(QStringLiteral("\\x1b")) && line.contains(QStringLiteral("\\n")),
                 qPrintable(line));
    }

    void methodIsEscaped() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QSignalSpy spy(&server, &mcp::McpServer::requestLog);
        post(server, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\\r\\nforged\"}");
        const QString line = requestLine(spy);
        QVERIFY2(!line.isEmpty(), "no request log line");
        QVERIFY2(!line.contains('\r') && !line.contains('\n'), qPrintable(line));
        QVERIFY2(line.contains(QStringLiteral("\\r\\n")), qPrintable(line));
    }

    // A code unit above 0xFF is written as \uHHHH, so a bidi override
    // cannot hide in the log or be misread as a shorter \x escape
    void wideCharactersUseUnicodeEscapes() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QSignalSpy spy(&server, &mcp::McpServer::requestLog);
        post(server, "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\","
                     "\"params\":{\"name\":\"a\\u202eevil\",\"arguments\":{}}}");
        const QString line = requestLine(spy);
        QVERIFY2(!line.contains(QChar(0x202e)), qPrintable(line));
        QVERIFY2(line.contains(QStringLiteral("\\u202eevil")), qPrintable(line));
    }

    // A refused request logs the client's HTTP method escaped too
    void refusedMethodIsEscaped() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QStringList lines;
        // Disconnected with `context` when this test returns
        QObject context;
        connect(logger::Logger::instance(), &logger::Logger::logLineWritten, &context,
                [&lines](const QString &line) { lines << line; });
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(client.waitForConnected(5000));
        client.write("PO\x1bST /mcp HTTP/1.1\r\nHost: evil.example\r\nContent-Length: 0\r\n\r\n");
        QTRY_VERIFY_WITH_TIMEOUT(lines.join('\n').contains(QStringLiteral("Refused")), 3000);
        for (const QString &line : std::as_const(lines)) {
            if (line.contains(QStringLiteral("Refused"))) {
                QVERIFY2(!line.contains(QChar(0x1b)), qPrintable(line));
                QVERIFY2(line.contains(QStringLiteral("\\x1b")), qPrintable(line));
            }
        }
    }

    // Ordinary names are logged unchanged
    void plainNamesAreUnchanged() {
        mcp::McpServer server;
        QVERIFY(server.start(0));
        QSignalSpy spy(&server, &mcp::McpServer::requestLog);
        post(server, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\","
                     "\"params\":{\"name\":\"list_sessions\",\"arguments\":{}}}");
        QVERIFY2(requestLine(spy).endsWith(QStringLiteral("-> tools/call: list_sessions")),
                 qPrintable(requestLine(spy)));
    }
};

QTEST_MAIN(TestMcpLogEscaping)
#include "test_mcp_log_escaping.moc"
