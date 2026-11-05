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

#include "McpHttpParser.h"

namespace mcp {

bool HttpRequest::wantsClose() const {
    // Connection is a comma-separated list of case-insensitive options
    bool close = false;
    bool keepAlive = false;
    for (const QString &option : headers.value("connection").split(',')) {
        const QString o = option.trimmed();
        close = close || o.compare(QLatin1String("close"), Qt::CaseInsensitive) == 0;
        keepAlive = keepAlive || o.compare(QLatin1String("keep-alive"), Qt::CaseInsensitive) == 0;
    }
    return close || (version == QLatin1String("HTTP/1.0") && !keepAlive);
}

int McpHttpParser::scanHeaderLines() {
    // A line ends with CRLF or a bare LF (RFC 9112 section 2.2 lets a
    // recipient accept LF alone). Empty lines before the request line are
    // skipped, as that section recommends. The scan resumes where the last
    // feed() stopped, so a client sending one byte at a time costs linear
    // time, not a rescan of the whole buffer per byte.
    while (true) {
        const int newline = m_buffer.indexOf('\n', m_scanPos);
        if (newline < 0)
            return -1;
        QByteArray line = m_buffer.mid(m_scanPos, newline - m_scanPos);
        if (line.endsWith('\r'))
            line.chop(1);
        m_scanPos = newline + 1;
        if (line.isEmpty()) {
            if (m_headerLines.isEmpty())
                continue;
            return m_scanPos;
        }
        m_headerLines.append(line);
    }
}

// Strip optional whitespace, which is only SP and HTAB (RFC 9110 5.6.3);
// QString::trimmed() would also drop VT, FF, NBSP and NEL
static QByteArray trimOws(const QByteArray &value) {
    int begin = 0;
    int end = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t'))
        ++begin;
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t'))
        --end;
    return value.mid(begin, end - begin);
}

bool McpHttpParser::fail(int status, const QString &message) {
    m_error = true;
    m_errorStatus = status;
    m_errorMessage = message;
    return false;
}

bool McpHttpParser::feed(const QByteArray &data) {
    if (m_error) return false;
    m_buffer.append(data);

    if (!m_headersParsed) {
        const int headerEnd = scanHeaderLines();
        const QList<QByteArray> &lines = m_headerLines;
        if (headerEnd < 0) {
            if (m_buffer.size() > kMaxHeaderSize) {
                m_error = true;
                m_errorMessage = QStringLiteral("Request header exceeds maximum size");
            }
            return false;
        }

        if (headerEnd > kMaxHeaderSize) {
            m_error = true;
            m_errorMessage = QStringLiteral("Request header exceeds maximum size");
            return false;
        }

        m_bodyStart = headerEnd;

        // A bare CR is not a line terminator (RFC 9112 section 2.2). Taking
        // it as data would let another HTTP hop split the line differently.
        for (const QByteArray &line : std::as_const(lines)) {
            if (line.contains('\r'))
                return fail(400, QStringLiteral("Bare CR in request header"));
        }

        // Parse request line
        QList<QByteArray> parts = lines.first().split(' ');
        if (parts.size() >= 2) {
            m_request.method = QString::fromLatin1(parts[0]);
            m_request.path = QString::fromLatin1(parts[1]);
        }
        if (parts.size() >= 3)
            m_request.version = QString::fromLatin1(parts[2]);

        // Parse headers. Anything that another HTTP hop could read as a
        // different header block is refused (RFC 9112 sections 5.1, 5.2).
        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray &line = lines[i];
            if (line.startsWith(' ') || line.startsWith('\t'))
                return fail(400, QStringLiteral("Folded header lines are not supported"));
            const int colon = line.indexOf(':');
            if (colon <= 0)
                return fail(400, QStringLiteral("Malformed header line"));
            const QByteArray name = line.left(colon);
            if (name.contains(' ') || name.contains('\t'))
                return fail(400, QStringLiteral("Whitespace before a header colon"));
            const QString key = QString::fromLatin1(name).toLower();
            const QString value = QString::fromLatin1(trimOws(line.mid(colon + 1)));
            if (key == QLatin1String("content-length") && m_request.headers.contains(key))
                return fail(400, QStringLiteral("Repeated Content-Length header"));
            m_request.headers[key] = value;
        }

        // Only Content-Length framing is implemented. A chunked (or any
        // other) Transfer-Encoding body would otherwise be read as an empty
        // body followed by its chunk lines as a second request, so refuse it
        // and let the caller close the connection.
        if (m_request.headers.contains("transfer-encoding")) {
            m_error = true;
            m_errorStatus = 501;
            m_errorMessage = QStringLiteral("Transfer-Encoding is not supported; send Content-Length");
            return false;
        }

        // Content-Length is 1*DIGIT: no sign, no list, no spaces inside
        const QString lengthHeader = m_request.headers.value("content-length", "0");
        bool digitsOnly = !lengthHeader.isEmpty();
        for (const QChar c : lengthHeader)
            digitsOnly = digitsOnly && c >= QLatin1Char('0') && c <= QLatin1Char('9');
        if (!digitsOnly)
            return fail(400, QStringLiteral("Invalid Content-Length"));
        bool lengthOk = false;
        const qint64 length = lengthHeader.toLongLong(&lengthOk);
        if (!lengthOk || length > kMaxBodySize)
            return fail(413, QStringLiteral("Content-Length exceeds maximum size"));
        m_contentLength = int(length);
        m_headersParsed = true;
    }

    // Check if we have the full body
    if (m_headersParsed) {
        int available = m_buffer.size() - m_bodyStart;
        if (available > kMaxBodySize) {
            m_error = true;
            m_errorMessage = QStringLiteral("Request body exceeds maximum size");
            return false;
        }
        if (available >= m_contentLength) {
            m_request.body = m_buffer.mid(m_bodyStart, m_contentLength);
            return true;
        }
    }

    return false;
}

void McpHttpParser::reset() {
    // "Reset for the next request" — drop only the bytes consumed by the
    // just-parsed request, keeping any leftover that already sits in the
    // buffer (e.g. a pipelined second request that arrived in the same
    // TCP read as the first request body). Wiping m_buffer wholesale
    // would silently discard those bytes.
    if (m_headersParsed && m_bodyStart >= 0) {
        const int consumed = m_bodyStart + m_contentLength;
        if (consumed > 0 && consumed <= m_buffer.size()) {
            m_buffer.remove(0, consumed);
        } else {
            // Parser state inconsistent (shouldn't happen on the success
            // path that triggers reset); clear to recover safely.
            m_buffer.clear();
        }
    } else {
        m_buffer.clear();
    }
    m_request = {};
    m_headerLines.clear();
    m_scanPos = 0;
    m_headersParsed = false;
    m_contentLength = 0;
    m_bodyStart = -1;
    m_error = false;
    m_errorStatus = 413;
    m_errorMessage.clear();
}

QByteArray HttpResponse::toBytes() const {
    QByteArray result;
    result.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toLatin1());
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        result.append(QString("%1: %2\r\n").arg(it.key(), it.value()).toLatin1());
    }
    if (!headers.contains("Content-Length") && !headers.contains("Transfer-Encoding")) {
        result.append(QString("Content-Length: %1\r\n").arg(body.size()).toLatin1());
    }
    // No Connection header by default: HTTP/1.1 connections are persistent,
    // and the server only closes after responses that set "close"
    result.append("\r\n");
    result.append(body);
    return result;
}

} // namespace mcp
