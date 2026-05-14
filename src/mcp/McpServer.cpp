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

#include "McpServer.h"
#include "McpHttpParser.h"
#include "logger/logger.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QPointer>
#include <QTcpSocket>
#include <QUuid>

#define MCP_LOG(msg) logger::Logger::instance()->debug_with_prefix("[MCP]", msg)
#define MCP_INFO(msg) logger::Logger::instance()->info_with_prefix("[MCP]", msg)
#define MCP_ERROR(msg) logger::Logger::instance()->error_with_prefix("[MCP]", msg)

/// Escape non-printable characters as \xHH and wrap in double quotes.
static QString sanitizeForLog(const QString &s, int maxLen = 80) {
    QString out;
    out.reserve(qMin(s.length(), maxLen) * 2 + 2);
    out += '"';
    int count = 0;
    for (const QChar &ch : s) {
        if (count >= maxLen) { out += QStringLiteral("..."); break; }
        ushort code = ch.unicode();
        if (code >= 0x20 && code < 0x7F) {
            if (ch == '"') out += QStringLiteral("\\\"");
            else if (ch == '\\') out += QStringLiteral("\\\\");
            else out += ch;
        } else if (ch == '\n') {
            out += QStringLiteral("\\n");
        } else if (ch == '\r') {
            out += QStringLiteral("\\r");
        } else if (ch == '\t') {
            out += QStringLiteral("\\t");
        } else {
            out += QStringLiteral("\\x") + QString::number(code, 16).rightJustified(2, '0');
        }
        ++count;
    }
    out += '"';
    return out;
}

namespace mcp {

McpServer::McpServer(QObject *parent)
    : QObject(parent),
      m_server(new QTcpServer(this)),
      m_registry(new McpSessionRegistry(this)),
      m_toolHandler(new McpToolHandler(m_registry, this)) {
    connect(m_server, &QTcpServer::newConnection, this, &McpServer::onNewConnection);
    connect(m_toolHandler, &McpToolHandler::createSessionRequested,
            this, &McpServer::createSessionRequested);
    connect(m_toolHandler, &McpToolHandler::closeSessionRequested,
            this, &McpServer::closeSessionRequested);
}

void McpServer::onSessionCreated(const QString &sessionId,
                                 ui::widgets::Q5250ScreenWidget *widget) {
    MCP_LOG(QString("Session created: %1").arg(sessionId));
    m_toolHandler->onSessionCreated(sessionId, widget);
}

void McpServer::onSessionClosed(const QString &sessionId) {
    MCP_LOG(QString("Session closed (user): %1").arg(sessionId));
    m_registry->removeSession(sessionId);
}

void McpServer::updateSessionStatus(const QString &sessionId, bool connected) {
    MCP_LOG(QString("Session %1: %2").arg(sessionId, connected ? "connected" : "disconnected"));
    m_registry->setConnected(sessionId, connected);
}

bool McpServer::start(quint16 port) {
    if (m_server->isListening()) return true;

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        MCP_ERROR(QString("Failed to start: %1").arg(m_server->errorString()));
        emit error("MCP server failed to start: " + m_server->errorString());
        return false;
    }
    MCP_INFO(QString("Listening on localhost:%1").arg(m_server->serverPort()));
    emit started(m_server->serverPort());
    return true;
}

void McpServer::stop() {
    if (m_server->isListening()) {
        MCP_INFO("Stopping server");
        m_server->close();
        m_initialized = false;
        m_sessionId.clear();
        emit stopped();
    }
}

bool McpServer::isRunning() const {
    return m_server->isListening();
}

quint16 McpServer::port() const {
    return m_server->serverPort();
}

void McpServer::onNewConnection() {
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        MCP_LOG(QString("New connection from %1:%2")
                    .arg(socket->peerAddress().toString()).arg(socket->peerPort()));
        // QueuedConnection is CRITICAL here.  Tool handlers (run_script,
        // create_session) enter a nested QEventLoop.  During that nested
        // loop, the client may disconnect and Qt will process the
        // resulting DeferredDelete event — destroying the socket while
        // its own readyRead signal emission is still active on the outer
        // call stack.  A direct (auto) connection would then crash in
        // Qt's signal dispatcher when the nested loop returns and the
        // dispatcher tries to touch the freed sender metadata.  A queued
        // connection decouples our slot from the emission stack, so the
        // socket can be safely destroyed mid-tool-call.  This also
        // prevents a second request arriving on a keep-alive connection
        // from re-entering onClientData and corrupting the shared parser.
        connect(socket, &QTcpSocket::readyRead, this, &McpServer::onClientData,
                Qt::QueuedConnection);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void McpServer::onClientData() {
    QPointer<QTcpSocket> socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;

    // Store parser as a property on the socket
    auto *parser = socket->findChild<QObject *>("_mcp_parser");
    McpHttpParser *httpParser;
    if (!parser) {
        auto *wrapper = new QObject(socket);
        wrapper->setObjectName("_mcp_parser");
        httpParser = new McpHttpParser();
        wrapper->setProperty("ptr", QVariant::fromValue(reinterpret_cast<quintptr>(httpParser)));
        connect(socket, &QObject::destroyed, wrapper, [httpParser]() { delete httpParser; });
    } else {
        httpParser = reinterpret_cast<McpHttpParser *>(parser->property("ptr").value<quintptr>());
    }

    // Local helper: write the same 413 error response and disconnect the
    // socket. Used by both the first-request error branch and the
    // pipelined-drain error branch so the two paths stay in sync.
    auto sendParseError = [&]() {
        MCP_ERROR(QString("HTTP parse error: %1").arg(httpParser->errorMessage()));
        HttpResponse resp;
        resp.statusCode = 413;
        resp.statusText = QStringLiteral("Payload Too Large");
        resp.headers["Content-Type"] = "application/json";
        QJsonObject err;
        err["error"] = httpParser->errorMessage();
        resp.body = QJsonDocument(err).toJson(QJsonDocument::Compact);
        socket->write(resp.toBytes());
        socket->flush();
        socket->disconnectFromHost();
    };

    if (httpParser->feed(socket->readAll())) {
        handleHttpRequest(socket);
        // The socket (and its httpParser) may have been destroyed during
        // handleHttpRequest if a nested event loop ran and the client
        // disconnected.  Only reset the parser if the socket is still alive.
        if (!socket.isNull())
            httpParser->reset();
        // Drain any additional requests that arrived in the same TCP read.
        // Now that reset() preserves the parser's leftover bytes (everything
        // past the just-parsed body), feed(empty) re-runs the state machine
        // on what is already buffered and returns true once another full
        // request is available.
        while (!socket.isNull() && httpParser->feed(QByteArray())) {
            handleHttpRequest(socket);
            if (!socket.isNull()) httpParser->reset();
        }
        // A malformed pipelined request (oversized header, bad
        // Content-Length, oversized body) leaves the parser in error
        // state and breaks out of the drain loop. Respond with the same
        // 413 + close as the first-request error branch, otherwise the
        // client is left waiting for a response that never comes.
        if (!socket.isNull() && httpParser->hasError()) {
            sendParseError();
        }
    } else if (httpParser->hasError()) {
        sendParseError();
    }
}

void McpServer::handleHttpRequest(QTcpSocket *sock) {
    // Guard with QPointer: tool calls (e.g. run_script) enter a nested
    // QEventLoop, during which the client may disconnect and the socket
    // gets destroyed via deleteLater.  Without this guard, writing the
    // response after the tool call returns would be a use-after-free.
    QPointer<QTcpSocket> socket = sock;

    auto *wrapper = socket->findChild<QObject *>("_mcp_parser");
    auto *parser = reinterpret_cast<McpHttpParser *>(wrapper->property("ptr").value<quintptr>());
    HttpRequest req = parser->request();

    MCP_LOG(QString("HTTP %1 %2").arg(req.method, req.path));

    // Accept requests to /mcp or / (some MCP clients use the root)
    if (req.path != "/mcp" && req.path != "/") {
        HttpResponse resp;
        resp.statusCode = 404;
        resp.statusText = "Not Found";
        resp.headers["Content-Type"] = "application/json";
        resp.body = R"({"error":"Not found"})";
        socket->write(resp.toBytes());
        socket->flush();
        return;
    }

    // Handle DELETE (session termination)
    if (req.method == "DELETE") {
        HttpResponse resp;
        resp.statusCode = 202;
        resp.statusText = "Accepted";
        socket->write(resp.toBytes());
        socket->flush();
        m_initialized = false;
        m_sessionId.clear();
        return;
    }

    // Handle GET (health check / SSE endpoint)
    if (req.method == "GET") {
        HttpResponse resp;
        resp.statusCode = 200;
        resp.headers["Content-Type"] = "application/json";
        resp.body = R"({"status":"ok","server":"5250ng-mcp","version":"1.0.0"})";
        socket->write(resp.toBytes());
        socket->flush();
        return;
    }

    // Only POST for JSON-RPC
    if (req.method != "POST") {
        HttpResponse resp;
        resp.statusCode = 405;
        resp.statusText = "Method Not Allowed";
        resp.headers["Allow"] = "GET, POST, DELETE";
        socket->write(resp.toBytes());
        socket->flush();
        return;
    }

    // Parse JSON-RPC request
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(req.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        MCP_ERROR(QString("JSON parse error: %1").arg(parseError.errorString()));
        QJsonObject err = jsonRpcError(QJsonValue::Null, -32700, "Parse error: " + parseError.errorString());
        HttpResponse resp;
        resp.statusCode = 200;
        resp.headers["Content-Type"] = "application/json";
        resp.body = QJsonDocument(err).toJson(QJsonDocument::Compact);
        socket->write(resp.toBytes());
        socket->flush();
        return;
    }

    QJsonObject rpcRequest = doc.object();
    QJsonObject rpcResponse = handleJsonRpc(rpcRequest);

    // The client may have disconnected while a tool call was running
    // inside a nested event loop (e.g. run_script, create_session).
    if (socket.isNull()) {
        MCP_LOG("Client disconnected before response could be sent");
        return;
    }

    HttpResponse resp;
    resp.statusCode = 200;
    resp.headers["Content-Type"] = "application/json";
    if (!m_sessionId.isEmpty())
        resp.headers["Mcp-Session-Id"] = m_sessionId;
    // Only send body if there is a response (notifications have no response)
    if (!rpcResponse.isEmpty()) {
        resp.body = QJsonDocument(rpcResponse).toJson(QJsonDocument::Compact);
    } else {
        resp.statusCode = 202;
        resp.statusText = "Accepted";
    }
    socket->write(resp.toBytes());
    socket->flush();
}

QJsonObject McpServer::handleJsonRpc(const QJsonObject &request) {
    QString method = request.value("method").toString();
    QJsonValue id = request.value("id");
    QJsonObject params = request.value("params").toObject();

    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");

    // Build log prefix
    QString logMethod = method;
    if (method == "tools/call")
        logMethod += ": " + params.value("name").toString();

    emit requestLog(QString("[%1] -> %2").arg(ts, logMethod));

    // Notifications (no id) — no response expected
    if (id.isUndefined() || id.isNull()) {
        return {};
    }

    QJsonObject response;
    if (method == "initialize")        response = jsonRpcResult(id, handleInitialize(params));
    else if (method == "ping")         response = jsonRpcResult(id, handlePing());
    else if (method == "tools/list")   response = jsonRpcResult(id, handleToolsList(params));
    else if (method == "tools/call")   response = jsonRpcResult(id, handleToolsCall(params));
    else                               response = jsonRpcError(id, -32601, "Method not found: " + method);

    // Log the response
    bool isError = response.contains("error");
    if (isError) {
        QString errMsg = response["error"].toObject()["message"].toString();
        emit requestLog(QString("[%1] <- ERROR: %2").arg(ts, sanitizeForLog(errMsg)));
    } else {
        QJsonObject result = response["result"].toObject();
        if (method == "tools/call") {
            bool toolError = result.value("isError").toBool();
            QJsonArray content = result.value("content").toArray();
            QString preview;
            if (!content.isEmpty())
                preview = sanitizeForLog(content[0].toObject()["text"].toString());
            emit requestLog(QString("[%1] <- %2 (%3)")
                .arg(ts, toolError ? "TOOL ERROR" : "OK", preview));
        } else if (method == "tools/list") {
            int count = result.value("tools").toArray().size();
            emit requestLog(QString("[%1] <- OK (%2 tools)").arg(ts).arg(count));
        } else {
            emit requestLog(QString("[%1] <- OK").arg(ts));
        }
    }

    return response;
}

QJsonObject McpServer::handleInitialize(const QJsonObject &params) {
    Q_UNUSED(params);
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_initialized = true;
    MCP_LOG(QString("Initialized, protocol session: %1").arg(m_sessionId));

    QJsonObject capabilities;
    QJsonObject toolsCap;
    toolsCap["listChanged"] = false;
    capabilities["tools"] = toolsCap;

    QJsonObject serverInfo;
    serverInfo["name"] = "5250ng-mcp";
    serverInfo["version"] = "1.0.0";

    QJsonObject result;
    result["protocolVersion"] = "2025-03-26";
    result["capabilities"] = capabilities;
    result["serverInfo"] = serverInfo;
    return result;
}

QJsonObject McpServer::handlePing() {
    return QJsonObject();
}

QJsonObject McpServer::handleToolsList(const QJsonObject &params) {
    Q_UNUSED(params);
    QJsonObject result;
    result["tools"] = m_toolHandler->listTools();
    return result;
}

QJsonObject McpServer::handleToolsCall(const QJsonObject &params) {
    QString name = params.value("name").toString();
    QJsonObject arguments = params.value("arguments").toObject();

    if (name.isEmpty())
        return m_toolHandler->makeResult("No tool name provided.", true);

    return m_toolHandler->callTool(name, arguments);
}

QJsonObject McpServer::jsonRpcResult(const QJsonValue &id, const QJsonObject &result) {
    QJsonObject resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["result"] = result;
    return resp;
}

QJsonObject McpServer::jsonRpcError(const QJsonValue &id, int code, const QString &message) {
    QJsonObject err;
    err["code"] = code;
    err["message"] = message;

    QJsonObject resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["error"] = err;
    return resp;
}

} // namespace mcp
