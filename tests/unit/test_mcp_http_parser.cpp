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
#include <QElapsedTimer>
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

    // Regression: a malformed pipelined second request must leave the
    // parser in an observable error state after reset(), so the server
    // can respond with the same 413 + disconnect path it uses on a
    // first-request error. Before this fix, reset() wiped the buffer and
    // the malformed bytes disappeared entirely.
    void resetExposesErrorOnMalformedPipelinedRequest() {
        McpHttpParser p;
        const QByteArray good = "POST /mcp HTTP/1.1\r\n"
                                "Content-Length: 2\r\n"
                                "\r\n"
                                "{}";
        const QByteArray bad = "POST /mcp HTTP/1.1\r\n"
                               "Content-Length: -7\r\n\r\n";
        QVERIFY(p.feed(good + bad));
        QCOMPARE(p.request().body, QByteArray("{}"));

        p.reset();
        QVERIFY(!p.hasError());

        // feed(empty) drains the buffered malformed request; the parser
        // must surface the error rather than silently swallow the bytes.
        QVERIFY(!p.feed(QByteArray()));
        QVERIFY(p.hasError());
    }
    // Issue #334: framings RFC 9112 requires a server to reject, and bare-LF
    // line endings, which must end lines unambiguously
    void rejectsMalformedFraming_data() {
        QTest::addColumn<QByteArray>("request");
        QTest::newRow("differing Content-Length")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length: 0\r\nContent-Length: 2\r\n\r\n{}");
        QTest::newRow("repeated Content-Length")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}");
        QTest::newRow("Content-Length list")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length: 2, 2\r\n\r\n{}");
        QTest::newRow("signed Content-Length")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length: +2\r\n\r\n{}");
        QTest::newRow("empty Content-Length")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length:\r\n\r\n");
        QTest::newRow("folded header")
            << QByteArray("POST /mcp HTTP/1.1\r\nX-A: 1\r\n  continued\r\nContent-Length: 2\r\n\r\n{}");
        QTest::newRow("header without colon")
            << QByteArray("POST /mcp HTTP/1.1\r\nPOST /mcp HTTP/1.1\r\nContent-Length: 2\r\n\r\n{}");
        QTest::newRow("space before colon")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length : 2\r\n\r\n{}");
        // Only SP and HTAB are optional whitespace (RFC 9110 5.6.3)
        QTest::newRow("VT before digits")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length: \x0b" "2\r\n\r\n{}");
        QTest::newRow("FF after digits")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length: 2\x0c\r\n\r\n{}");
        QTest::newRow("NBSP before digits")
            << QByteArray("POST /mcp HTTP/1.1\r\nContent-Length: \xa0" "2\r\n\r\n{}");
        QTest::newRow("bare CR")
            << QByteArray("POST /mcp HTTP/1.1\r\nX-A: 1\rContent-Length: 2\r\n\r\n{}");
    }
    void rejectsMalformedFraming() {
        QFETCH(QByteArray, request);
        McpHttpParser p;
        QVERIFY(!p.feed(request));
        QVERIFY(p.hasError());
        QCOMPARE(p.errorStatusCode(), 400);
    }

    void hugeDigitContentLengthIs413() {
        McpHttpParser p;
        QVERIFY(!p.feed("POST /mcp HTTP/1.1\r\nContent-Length: 99999999999999999999999\r\n\r\n"));
        QCOMPARE(p.errorStatusCode(), 413);
    }

    void leadingZerosAreDigits() {
        McpHttpParser p;
        QVERIFY(p.feed("POST /mcp HTTP/1.1\r\nContent-Length: 002\r\n\r\n{}"));
        QCOMPARE(p.request().body, QByteArray("{}"));
    }

    // Bare LF ends lines and the header block, instead of buffering to the
    // header limit and answering 413
    void acceptsBareLfLineEndings() {
        McpHttpParser p;
        QVERIFY(p.feed("POST /mcp HTTP/1.1\nHost: x\nContent-Length: 2\n\n{}"));
        QVERIFY(!p.hasError());
        QCOMPARE(p.request().method, QStringLiteral("POST"));
        QCOMPARE(p.request().headers.value("host"), QStringLiteral("x"));
        QCOMPARE(p.request().body, QByteArray("{}"));
    }

    // A request ending with LF LF is complete there; the next request's
    // headers do not merge into it
    void mixedLineEndingsDoNotMergeRequests() {
        McpHttpParser p;
        const QByteArray first = "POST /mcp HTTP/1.1\nContent-Length: 2\n\n{}";
        const QByteArray second = "POST /mcp HTTP/1.1\r\nX-Second: yes\r\nContent-Length: 4\r\n\r\n[12]";
        QVERIFY(p.feed(first + second));
        QCOMPARE(p.request().body, QByteArray("{}"));
        QVERIFY(!p.request().headers.contains("x-second"));
        p.reset();
        QVERIFY(p.feed(QByteArray()));
        QCOMPARE(p.request().headers.value("x-second"), QStringLiteral("yes"));
        QCOMPARE(p.request().body, QByteArray("[12]"));
    }

    // An empty line before the request line is skipped (RFC 9112 2.2)
    void skipsEmptyLineBeforeRequestLine() {
        McpHttpParser p;
        QVERIFY(p.feed("\r\nPOST /mcp HTTP/1.1\r\nContent-Length: 2\r\n\r\n{}"));
        QCOMPARE(p.request().method, QStringLiteral("POST"));
        QCOMPARE(p.request().body, QByteArray("{}"));
    }

    // A complete header block over the limit is refused even when it
    // arrives in one read, blank line included
    void oversizedCompleteHeaderInOneRead() {
        McpHttpParser p;
        QByteArray req = "POST /mcp HTTP/1.1\r\nX-Pad: "
                       + QByteArray(McpHttpParser::kMaxHeaderSize + 1, 'a') + "\r\n\r\n";
        QVERIFY(!p.feed(req));
        QVERIFY(p.hasError());
        QCOMPARE(p.errorStatusCode(), 413);
    }

    // SP and HTAB around a value are stripped; anything inside is kept
    void stripsOnlySpaceAndTabAroundValues() {
        McpHttpParser p;
        QVERIFY(p.feed("POST /mcp HTTP/1.1\r\nX-A: \t a \x0b b \t\r\nContent-Length:\t2 \r\n\r\n{}"));
        QCOMPARE(p.request().headers.value("x-a"), QString::fromLatin1("a \x0b b"));
        McpHttpParser q;
        QVERIFY(q.feed("POST /mcp HTTP/1.1\r\nX-B: b\x0b\r\nX-C: \xa0" "c\r\nContent-Length: 0\r\n\r\n"));
        QCOMPARE(q.request().headers.value("x-b"), QString::fromLatin1("b\x0b"));
        QCOMPARE(q.request().headers.value("x-c"), QString::fromLatin1("\xa0" "c"));
        QCOMPARE(p.request().body, QByteArray("{}"));
    }

    // A header block sent one byte at a time is scanned incrementally: it
    // reaches the size limit in linear time instead of rescanning the
    // buffer on every byte (about 18 s for 64 KiB before)
    void byteAtATimeHeaderIsLinear() {
        McpHttpParser p;
        QByteArray stream = "POST /mcp HTTP/1.1\r\n";
        while (stream.size() <= McpHttpParser::kMaxHeaderSize + 16)
            stream += "a:b\r\n";
        QElapsedTimer timer;
        timer.start();
        int fed = 0;
        for (; fed < stream.size() && !p.hasError(); ++fed)
            QVERIFY(!p.feed(stream.mid(fed, 1)));
        const qint64 elapsed = timer.elapsed();
        qInfo("fed %d bytes one at a time in %lld ms", fed, elapsed);
        QVERIFY(p.hasError());
        QCOMPARE(p.errorStatusCode(), 413);
        QVERIFY2(elapsed < 3000, qPrintable(QString::number(elapsed)));
    }

    // Headers split across reads still parse once the blank line arrives
    void headerBlockAcrossReads() {
        McpHttpParser p;
        QVERIFY(!p.feed("POST /mcp HTTP/1.1\r\nContent-Len"));
        QVERIFY(!p.feed("gth: 2\r\n"));
        QVERIFY(!p.feed("\r"));
        QVERIFY(!p.hasError());
        QVERIFY(p.feed("\n{}"));
        QCOMPARE(p.request().body, QByteArray("{}"));
    }
};

QTEST_MAIN(TestMcpHttpParser)
#include "test_mcp_http_parser.moc"
