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

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>

namespace ui::widgets { class Q5250ScreenWidget; }

namespace mcp {

class McpSessionRegistry;

/// Executes MCP tool calls, accessing terminal sessions and the filesystem.
class McpToolHandler : public QObject {
    Q_OBJECT
  public:
    explicit McpToolHandler(McpSessionRegistry *registry, QObject *parent = nullptr);

    /// Returns MCP-formatted tool list for tools/list response.
    QJsonArray listTools() const;

    /// Execute a tool and return the MCP tools/call result object.
    QJsonObject callTool(const QString &name, const QJsonObject &arguments);

    QJsonObject makeResult(const QString &text, bool isError = false) const;

    /// Called by McpServer when MainWindow has created the session tab.
    void onSessionCreated(const QString &sessionId,
                          ui::widgets::Q5250ScreenWidget *widget);

  signals:
    void createSessionRequested(const QString &sessionId, const QString &hostname,
                                quint16 port, bool useTLS);
    void closeSessionRequested(const QString &sessionId);

  private:
    /// Look up session widget. Returns nullptr and sets error result if invalid.
    ui::widgets::Q5250ScreenWidget *resolveSession(const QJsonObject &args,
                                                    QJsonObject &errorResult);

    // Session lifecycle tools
    QJsonObject handleCreateSession(const QJsonObject &args);
    QJsonObject handleCloseSession(const QJsonObject &args);
    QJsonObject handleListSessions();

    // Screen tools (session-aware)
    QJsonObject handleReadScreen(const QJsonObject &args);
    QJsonObject handleReadLine(const QJsonObject &args);
    QJsonObject handleReadRegion(const QJsonObject &args);
    QJsonObject handleGetCursorPosition(const QJsonObject &args);
    QJsonObject handleGetScreenSize(const QJsonObject &args);
    QJsonObject handleGetFieldAt(const QJsonObject &args);
    QJsonObject handleFindText(const QJsonObject &args);
    QJsonObject handleScreenshot(const QJsonObject &args);

    // Script tools (session-aware)
    QJsonObject handleSendKeys(const QJsonObject &args);
    QJsonObject handlePressKey(const QJsonObject &args);
    QJsonObject handlePressKeys(const QJsonObject &args);
    QJsonObject handleTypeText(const QJsonObject &args);
    QJsonObject handleSetCursorPosition(const QJsonObject &args);
    QJsonObject handleMoveCursor(const QJsonObject &args);
    QJsonObject handleWaitForText(const QJsonObject &args);
    QJsonObject handleRunScript(const QJsonObject &args);
    QJsonObject handleLogin(const QJsonObject &args);
    QJsonObject handleClearInputs(const QJsonObject &args);

    // Filesystem tools (session-independent)
    QJsonObject handleListFiles(const QJsonObject &args);
    QJsonObject handleReadFile(const QJsonObject &args);
    QJsonObject handleWriteFile(const QJsonObject &args);

    /// Read screen text from a specific widget (thread-safe).
    QString readScreenText(ui::widgets::Q5250ScreenWidget *widget);

    McpSessionRegistry *m_registry;

    // Pending create_session state
    QString m_pendingSessionId;
    bool m_sessionCreated = false;

    // Per-session busy guard: tracks which sessions are inside a nested
    // event loop (e.g. handleRunScript, handleCreateSession).  Tool calls
    // targeting a different session, or session-independent tools, proceed
    // without blocking.
    QSet<QString> m_busySessions;
};

} // namespace mcp
