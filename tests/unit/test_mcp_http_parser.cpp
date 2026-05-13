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

#include "mcp/McpHttpParser.h"
#include <QByteArray>
#include <QTest>

using namespace mcp;

class TestMcpHttpParser : public QObject {
    Q_OBJECT

  private slots:
    void parsesSimpleRequest() {
        McpHttpParser p;
        QByteArray req = "POST /mcp HTTP/1.1\r\n"
                         "Host: localhost\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: 2\r\n"
                         "\r\n"
                         "{}";
        QVERIFY(p.feed(req));
        QVERIFY(!p.hasError());
        QCOMPARE(p.request().method, QStringLiteral("POST"));
        QCOMPARE(p.request().path, QStringLiteral("/mcp"));
        QCOMPARE(p.request().body, QByteArray("{}"));
    }

    void rejectsOversizedHeaderBlock() {
        McpHttpParser p;
        // Send header bytes beyond the parser's kMaxHeaderSize limit,
        // without the terminating \r\n\r\n, to simulate a slow-loris
        // style client that would otherwise grow the buffer unboundedly.
        QByteArray big = QByteArray("GET / HTTP/1.1\r\nX-Pad: ")
                         + QByteArray(McpHttpParser::kMaxHeaderSize + 1, 'a');
        QVERIFY(!p.feed(big));
        QVERIFY(p.hasError());
        QVERIFY(!p.errorMessage().isEmpty());
    }

    void rejectsOversizedContentLength() {
        McpHttpParser p;
        QByteArray req = "POST /mcp HTTP/1.1\r\n"
                         "Content-Length: "
                       + QByteArray::number(static_cast<qint64>(McpHttpParser::kMaxBodySize) + 1)
                       + "\r\n\r\n";
        QVERIFY(!p.feed(req));
        QVERIFY(p.hasError());
    }

    void rejectsNegativeContentLength() {
        McpHttpParser p;
        QByteArray req = "POST /mcp HTTP/1.1\r\n"
                         "Content-Length: -1\r\n\r\n";
        QVERIFY(!p.feed(req));
        QVERIFY(p.hasError());
    }

    void rejectsNonNumericContentLength() {
        McpHttpParser p;
        QByteArray req = "POST /mcp HTTP/1.1\r\n"
                         "Content-Length: lots\r\n\r\n";
        QVERIFY(!p.feed(req));
        QVERIFY(p.hasError());
    }

    void ignoresFurtherDataOnceErrored() {
        McpHttpParser p;
        QByteArray req = "POST /mcp HTTP/1.1\r\n"
                         "Content-Length: -5\r\n\r\n";
        QVERIFY(!p.feed(req));
        QVERIFY(p.hasError());
        // Additional bytes after the error must not be accepted and must not
        // grow the buffer.
        QVERIFY(!p.feed(QByteArray(1024, 'x')));
        QVERIFY(p.hasError());
    }

    void resetClearsErrorState() {
        McpHttpParser p;
        QByteArray req = "POST /mcp HTTP/1.1\r\n"
                         "Content-Length: -1\r\n\r\n";
        QVERIFY(!p.feed(req));
        QVERIFY(p.hasError());
        p.reset();
        QVERIFY(!p.hasError());
        QCOMPARE(p.errorMessage(), QString());
    }

    // Regression: reset() previously cleared the buffer wholesale, so a
    // pipelined second request that arrived in the same TCP read as the
    // first request body was silently discarded.
    void resetPreservesLeftoverForPipelinedRequest() {
        McpHttpParser p;
        const QByteArray first = "POST /mcp HTTP/1.1\r\n"
                                 "Content-Length: 2\r\n"
                                 "\r\n"
                                 "{}";
        const QByteArray second = "POST /mcp HTTP/1.1\r\n"
                                  "Content-Length: 4\r\n"
                                  "\r\n"
                                  "[12]";
        // Feed both requests in a single buffer, as readAll() would deliver
        // them on a busy socket.
        QVERIFY(p.feed(first + second));
        QCOMPARE(p.request().body, QByteArray("{}"));

        // After reset, the parser should still have the second request
        // buffered and feed(empty) should immediately return true.
        p.reset();
        QVERIFY(!p.hasError());
        QVERIFY(p.feed(QByteArray()));
        QCOMPARE(p.request().body, QByteArray("[12]"));

        // No third request → feed(empty) returns false without erroring.
        p.reset();
        QVERIFY(!p.feed(QByteArray()));
        QVERIFY(!p.hasError());
    }
};

QTEST_MAIN(TestMcpHttpParser)
#include "test_mcp_http_parser.moc"
