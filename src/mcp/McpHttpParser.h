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
    /// Feed raw TCP data. Returns true when a complete request is available.
    bool feed(const QByteArray &data);

    /// Returns the parsed request. Only valid after feed() returns true.
    HttpRequest request() const { return m_request; }

    /// Reset for the next request.
    void reset();

  private:
    QByteArray m_buffer;
    HttpRequest m_request;
    bool m_headersParsed = false;
    int m_contentLength = 0;
    int m_bodyStart = -1;
};

} // namespace mcp
