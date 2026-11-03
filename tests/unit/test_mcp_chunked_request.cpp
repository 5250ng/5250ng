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

// Only Content-Length framing is implemented. A Transfer-Encoding body must
// be refused, not read as an empty body followed by its chunk lines as a
// second request (request smuggling).
class TestMcpChunkedRequest : public QObject {
    Q_OBJECT

  private slots:
    void chunkedRequestIsRefused() {
        McpHttpParser p;
        const QByteArray req =
            "POST /mcp HTTP/1.1\r\n"
            "Host: x\r\n"
            "Content-Type: application/json\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "2a\r\n{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}  \r\n0\r\n\r\n";
        QVERIFY(!p.feed(req));
        QVERIFY(p.hasError());
        QCOMPARE(p.errorStatusCode(), 501);
        // Nothing more is parsed from the stream: the chunk lines never
        // become a request of their own.
        QVERIFY(!p.feed(QByteArray()));
    }

    void transferEncodingWithContentLengthIsRefused() {
        McpHttpParser p;
        QVERIFY(!p.feed("POST /mcp HTTP/1.1\r\n"
                        "Content-Length: 4\r\n"
                        "Transfer-Encoding: chunked\r\n"
                        "\r\n"
                        "0\r\n\r\n"));
        QVERIFY(p.hasError());
        QCOMPARE(p.errorStatusCode(), 501);
    }

    void errorStatusMatchesTheError() {
        McpHttpParser bad;
        QVERIFY(!bad.feed("POST /mcp HTTP/1.1\r\nContent-Length: abc\r\n\r\n"));
        QCOMPARE(bad.errorStatusCode(), 400);

        McpHttpParser big;
        QVERIFY(!big.feed("POST /mcp HTTP/1.1\r\nContent-Length: 999999999\r\n\r\n"));
        QCOMPARE(big.errorStatusCode(), 413);

        McpHttpParser ok;
        QVERIFY(ok.feed("POST /mcp HTTP/1.1\r\nContent-Length: 2\r\n\r\n{}"));
        QCOMPARE(ok.errorStatusCode(), 0);
    }
};

QTEST_GUILESS_MAIN(TestMcpChunkedRequest)
#include "test_mcp_chunked_request.moc"
