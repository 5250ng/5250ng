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

#include "McpSessionRegistry.h"
#include "McpToolHandler.h"
#include <QObject>
#include <QTcpServer>

namespace ui::widgets { class Q5250ScreenWidget; }

namespace mcp {

/// MCP server exposing 5250ng terminal tools over HTTP+JSON-RPC.
/// Listens on a TCP port and handles the MCP Streamable HTTP transport.
class McpServer : public QObject {
    Q_OBJECT
  public:
    explicit McpServer(QObject *parent = nullptr);

    bool start(quint16 port = 9250);
    void stop();
    bool isRunning() const;
    quint16 port() const;

    /// Called by MainWindow when an MCP session tab has been created.
    void onSessionCreated(const QString &sessionId,
                          ui::widgets::Q5250ScreenWidget *widget);

    /// Called by MainWindow when a user manually closes an MCP tab.
    void onSessionClosed(const QString &sessionId);

    /// Called by MainWindow when an MCP session's connection state changes.
    void updateSessionStatus(const QString &sessionId, bool connected);

  signals:
    void started(quint16 port);
    void stopped();
    void error(const QString &msg);
    /// Emitted when create_session tool is called.
    void createSessionRequested(const QString &sessionId, const QString &hostname,
                                quint16 port, bool useTLS);
    /// Emitted when close_session tool is called.
    void closeSessionRequested(const QString &sessionId);
    void requestLog(const QString &entry);

  private:
    void onNewConnection();
    void onClientData();
    void handleHttpRequest(QTcpSocket *socket);

    QJsonObject handleJsonRpc(const QJsonObject &request);
    QJsonObject handleInitialize(const QJsonObject &params);
    QJsonObject handleToolsList(const QJsonObject &params);
    QJsonObject handleToolsCall(const QJsonObject &params);
    QJsonObject handlePing();

    static QJsonObject jsonRpcResult(const QJsonValue &id, const QJsonObject &result);
    static QJsonObject jsonRpcError(const QJsonValue &id, int code, const QString &message);

    QTcpServer *m_server = nullptr;
    McpSessionRegistry *m_registry = nullptr;
    McpToolHandler *m_toolHandler = nullptr;
    QString m_sessionId; // JSON-RPC session ID (MCP protocol level)
    bool m_initialized = false;
};

} // namespace mcp
