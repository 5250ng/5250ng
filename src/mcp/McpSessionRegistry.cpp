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

#include "McpSessionRegistry.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"

namespace mcp {

McpSessionRegistry::McpSessionRegistry(QObject *parent) : QObject(parent) {}

void McpSessionRegistry::addSession(const McpSessionInfo &info) {
    m_sessions.insert(info.sessionId, info);
}

bool McpSessionRegistry::removeSession(const QString &sessionId) {
    if (m_sessions.remove(sessionId)) {
        emit sessionRemoved(sessionId);
        return true;
    }
    return false;
}

McpSessionInfo McpSessionRegistry::session(const QString &sessionId) const {
    auto it = m_sessions.constFind(sessionId);
    if (it == m_sessions.constEnd()) return {};
    return it.value();
}

QList<McpSessionInfo> McpSessionRegistry::allSessions() const {
    return m_sessions.values();
}

void McpSessionRegistry::setConnected(const QString &sessionId, bool connected) {
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end())
        it->connected = connected;
}

void McpSessionRegistry::setDisplayWidget(const QString &sessionId,
                                          ui::widgets::Q5250ScreenWidget *widget) {
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end())
        it->displayWidget = widget;
}

bool McpSessionRegistry::hasSession(const QString &sessionId) const {
    return m_sessions.contains(sessionId);
}

} // namespace mcp
