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

#include <QString>
#include <QUrl>

namespace mcp {

// Checks for requests reaching the localhost MCP server. A web page in the
// user's browser can send a CORS "simple request" to 127.0.0.1 (cross-site
// request forgery) or, after DNS rebinding, under its own host name; browsers
// always send Origin and Host for those, while MCP clients send neither a
// foreign Origin nor a foreign Host.

inline bool isLoopbackHostName(QString host) {
    host = host.toLower();
    if (host.startsWith('[') && host.endsWith(']'))
        host = host.mid(1, host.size() - 2);
    return host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
           || host == QLatin1String("::1");
}

/// Host header value ("host" or "host:port"); empty (an HTTP/1.0 client)
/// is accepted.
inline bool isAllowedHostHeader(const QString &value) {
    const QString host = value.trimmed();
    if (host.isEmpty())
        return true;
    const QUrl url(QStringLiteral("http://") + host);
    if (!url.isValid() || !url.userInfo().isEmpty() || !url.path().isEmpty())
        return false;
    return isLoopbackHostName(url.host());
}

/// Origin header value; absent (non-browser clients) is accepted, a page
/// served from anywhere but this machine is not.
inline bool isAllowedOrigin(const QString &value) {
    const QString origin = value.trimmed();
    if (origin.isEmpty())
        return true;
    const QUrl url(origin);
    if (!url.isValid()
        || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
        return false;  // includes the opaque "null" origin
    return isLoopbackHostName(url.host());
}

/// JSON-RPC bodies must be sent as application/json; text/plain and form
/// types are what a browser may send cross-site without a preflight.
inline bool isJsonContentType(const QString &value) {
    return value.section(';', 0, 0).trimmed().compare(QLatin1String("application/json"),
                                                      Qt::CaseInsensitive) == 0;
}

} // namespace mcp
