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

bool McpHttpParser::feed(const QByteArray &data) {
    if (m_error) return false;
    m_buffer.append(data);

    if (!m_headersParsed) {
        int headerEnd = m_buffer.indexOf("\r\n\r\n");
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

        m_bodyStart = headerEnd + 4;

        // Parse request line
        int firstLine = m_buffer.indexOf("\r\n");
        QByteArray requestLine = m_buffer.left(firstLine);
        QList<QByteArray> parts = requestLine.split(' ');
        if (parts.size() >= 2) {
            m_request.method = QString::fromLatin1(parts[0]);
            m_request.path = QString::fromLatin1(parts[1]);
        }

        // Parse headers
        QByteArray headerBlock = m_buffer.mid(firstLine + 2, headerEnd - firstLine - 2);
        for (const QByteArray &line : headerBlock.split('\n')) {
            QByteArray trimmed = line.trimmed();
            int colon = trimmed.indexOf(':');
            if (colon > 0) {
                QString key = QString::fromLatin1(trimmed.left(colon)).trimmed().toLower();
                QString value = QString::fromLatin1(trimmed.mid(colon + 1)).trimmed();
                m_request.headers[key] = value;
            }
        }

        bool lengthOk = false;
        const QString lengthHeader = m_request.headers.value("content-length", "0");
        m_contentLength = lengthHeader.toInt(&lengthOk);
        if (!lengthOk || m_contentLength < 0 || m_contentLength > kMaxBodySize) {
            m_error = true;
            m_errorMessage = QStringLiteral("Invalid or oversized Content-Length");
            return false;
        }
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
    m_buffer.clear();
    m_request = {};
    m_headersParsed = false;
    m_contentLength = 0;
    m_bodyStart = -1;
    m_error = false;
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
    if (!headers.contains("Connection")) {
        result.append("Connection: close\r\n");
    }
    result.append("\r\n");
    result.append(body);
    return result;
}

} // namespace mcp
