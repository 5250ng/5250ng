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

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

namespace ui::widgets { class Q5250ScreenWidget; }

namespace mcp {

struct McpSessionInfo {
    QString sessionId;
    QString hostname;
    quint16 port = 23;
    bool useTLS = false;
    ui::widgets::Q5250ScreenWidget *displayWidget = nullptr; // non-owning
    bool connected = false;
};

/// Registry mapping MCP session IDs to their display widgets and metadata.
class McpSessionRegistry : public QObject {
    Q_OBJECT
  public:
    explicit McpSessionRegistry(QObject *parent = nullptr);

    /// Register a session. The sessionId in info must already be set.
    void addSession(const McpSessionInfo &info);

    /// Remove a session by ID. Returns false if not found.
    bool removeSession(const QString &sessionId);

    /// Look up a session. Returns nullptr if not found.
    McpSessionInfo *session(const QString &sessionId);

    /// Return all registered sessions.
    QList<McpSessionInfo> allSessions() const;

    /// Update the connected status of a session.
    void setConnected(const QString &sessionId, bool connected);

    /// Update the display widget pointer for a session.
    void setDisplayWidget(const QString &sessionId,
                          ui::widgets::Q5250ScreenWidget *widget);

    bool hasSession(const QString &sessionId) const;

  signals:
    void sessionRemoved(const QString &sessionId);

  private:
    QHash<QString, McpSessionInfo> m_sessions;
};

} // namespace mcp
