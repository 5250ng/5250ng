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

#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>

namespace mcp {

struct HttpRequest {
    QString method;  // GET, POST, DELETE
    QString path;    // /mcp
    QMap<QString, QString> headers;
    QByteArray body;
};

struct HttpResponse {
    int statusCode = 200;
    QString statusText = "OK";
    QMap<QString, QString> headers;
    QByteArray body;

    QByteArray toBytes() const;
};

/// Minimal HTTP/1.1 request parser for MCP transport.
class McpHttpParser {
  public:
    /// Maximum bytes accepted before `\r\n\r\n` is seen.
    static constexpr int kMaxHeaderSize = 64 * 1024;
    /// Maximum body bytes accepted, both by Content-Length and by actual
    /// buffered payload.
    static constexpr int kMaxBodySize = 16 * 1024 * 1024;

    /// Feed raw TCP data. Returns true when a complete request is available.
    /// Returns false if the request is incomplete or if the parser has
    /// entered an error state; check hasError() to distinguish.
    bool feed(const QByteArray &data);

    /// Returns the parsed request. Only valid after feed() returns true.
    HttpRequest request() const { return m_request; }

    /// Reset for the next request.
    void reset();

    /// True once the parser has refused the stream (oversized or malformed).
    /// After an error the parser will not accept further data; the caller
    /// should close the connection.
    bool hasError() const { return m_error; }

    /// Human-readable reason for the current error, or empty if none.
    QString errorMessage() const { return m_errorMessage; }

  private:
    QByteArray m_buffer;
    HttpRequest m_request;
    bool m_headersParsed = false;
    int m_contentLength = 0;
    int m_bodyStart = -1;
    bool m_error = false;
    QString m_errorMessage;
};

} // namespace mcp
