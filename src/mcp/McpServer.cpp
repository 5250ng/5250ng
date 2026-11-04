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
#include "mcp_request_guard.h"
#include "logger/logger.h"
#include <QDateTime>
#include <QJsonArray>
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

void McpServer::setPasswordPrompt(McpToolHandler::PasswordPrompt prompt) {
    m_toolHandler->setPasswordPrompt(std::move(prompt));
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
        // close() only stops accepting new connections. Drop the clients
        // already connected too, or a keep-alive client could go on calling
        // tools after the user turned the server off. Accepted sockets are
        // children of the QTcpServer.
        const auto clients = m_server->findChildren<QTcpSocket *>(QString(), Qt::FindDirectChildrenOnly);
        for (QTcpSocket *socket : clients) {
            disconnect(socket, &QTcpSocket::readyRead, this, &McpServer::onClientData);
            socket->abort();
        }
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
        // socket can be safely destroyed mid-tool-call.  It does not stop
        // a tool's nested event loop from delivering readyRead again; the
        // busy guard in onClientData() handles that re-entry.
        connect(socket, &QTcpSocket::readyRead, this, &McpServer::onClientData,
                Qt::QueuedConnection);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void McpServer::onClientData() {
    QPointer<QTcpSocket> socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;
    // A readyRead queued before stop() must not run a request either.
    if (!m_server->isListening()) {
        socket->abort();
        return;
    }

    // A tool call running a nested event loop (scripts, wait_for_text)
    // lets this socket's readyRead in again before the outer call has reset
    // the parser, which would dispatch the in-flight request a second time.
    // Leave the new bytes in the socket: the outer call's drain loop below
    // reads them once the current request is done, keeping requests in order.
    if (socket->property("_mcp_busy").toBool())
        return;
    socket->setProperty("_mcp_busy", true);
    struct BusyGuard {
        QPointer<QTcpSocket> s;
        ~BusyGuard() { if (s) s->setProperty("_mcp_busy", false); }
    } busyGuard{socket};

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

    // Local helper: write the parser's error response and disconnect the
    // socket. Used by both the first-request error branch and the
    // pipelined-drain error branch so the two paths stay in sync.
    auto sendParseError = [&]() {
        MCP_ERROR(QString("HTTP parse error: %1").arg(httpParser->errorMessage()));
        HttpResponse resp;
        resp.statusCode = httpParser->errorStatusCode();
        switch (resp.statusCode) {
        case 400: resp.statusText = QStringLiteral("Bad Request"); break;
        case 501: resp.statusText = QStringLiteral("Not Implemented"); break;
        default:
            resp.statusCode = 413;
            resp.statusText = QStringLiteral("Payload Too Large");
            break;
        }
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
        // Drain any additional requests: those that arrived in the same TCP
        // read (reset() keeps the parser's leftover bytes past the parsed
        // body) and those that arrived while the request ran, which the
        // re-entry guard above left unread in the socket.
        while (!socket.isNull() && httpParser->feed(socket->readAll())) {
            handleHttpRequest(socket);
            if (!socket.isNull()) httpParser->reset();
        }
        // A malformed pipelined request (oversized header, bad
        // Content-Length, oversized body) leaves the parser in error
        // state and breaks out of the drain loop. Respond with the same
        // error response + close as the first-request error branch, otherwise the
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

    // Every request goes through here, including pipelined ones drained
    // from the parser after an earlier request's tool call (whose nested
    // event loop may have run stop()). None may run once the server is off.
    if (!m_server->isListening() || socket->state() != QAbstractSocket::ConnectedState) {
        socket->abort();
        return;
    }

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

    // Refuse requests from web pages: a foreign Origin (cross-site request)
    // or a foreign Host (DNS rebinding). Any page in the user's browser could
    // otherwise call the tools (files, sessions, typing into IBM i).
    auto refuse = [&](int code, const QString &text, const QString &reason) {
        MCP_ERROR(QString("Refused %1 request: %2").arg(req.method, reason));
        HttpResponse resp;
        resp.statusCode = code;
        resp.statusText = text;
        resp.headers["Content-Type"] = "application/json";
        QJsonObject err;
        err["error"] = reason;
        resp.body = QJsonDocument(err).toJson(QJsonDocument::Compact);
        socket->write(resp.toBytes());
        socket->flush();
        socket->disconnectFromHost();
    };
    if (!isAllowedHostHeader(req.headers.value("host"))) {
        refuse(403, QStringLiteral("Forbidden"), QStringLiteral("Host must be localhost"));
        return;
    }
    if (!isAllowedOrigin(req.headers.value("origin"))) {
        refuse(403, QStringLiteral("Forbidden"), QStringLiteral("Cross-origin requests are not allowed"));
        return;
    }
    if (req.method == "POST" && !isJsonContentType(req.headers.value("content-type"))) {
        refuse(415, QStringLiteral("Unsupported Media Type"),
               QStringLiteral("Content-Type must be application/json"));
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

    // Answer with a JSON-RPC response: an object, a batch array, or
    // nothing (only notifications), which is 202 Accepted with no body
    auto respond = [&](const QJsonDocument &rpc) {
        HttpResponse resp;
        resp.statusCode = 200;
        resp.headers["Content-Type"] = "application/json";
        if (!m_sessionId.isEmpty())
            resp.headers["Mcp-Session-Id"] = m_sessionId;
        if (!rpc.isNull()) {
            resp.body = rpc.toJson(QJsonDocument::Compact);
        } else {
            resp.statusCode = 202;
            resp.statusText = "Accepted";
        }
        socket->write(resp.toBytes());
        socket->flush();
    };

    // Parse JSON-RPC request
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(req.body, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        // QJsonDocument refuses a top-level scalar (1, "x", null) as an
        // illegal value, but that is valid JSON, just not a request
        QJsonParseError wrappedError;
        const QJsonDocument wrapped =
            QJsonDocument::fromJson("[" + req.body + "]", &wrappedError);
        if (wrappedError.error == QJsonParseError::NoError && wrapped.array().size() == 1) {
            respond(QJsonDocument(jsonRpcError(
                QJsonValue::Null, -32600,
                "Invalid Request: expected a JSON-RPC request object or a non-empty batch array")));
            return;
        }
        MCP_ERROR(QString("JSON parse error: %1").arg(parseError.errorString()));
        respond(QJsonDocument(
            jsonRpcError(QJsonValue::Null, -32700, "Parse error: " + parseError.errorString())));
        return;
    }

    // A batch (JSON-RPC 2.0 section 6, supported by MCP 2025-03-26):
    // each member is handled in order, and the responses that are not to
    // notifications are returned as one array
    if (doc.isArray() && !doc.array().isEmpty()) {
        // Each member costs a response, so a body of tiny members would
        // otherwise make the GUI thread build a response many times its size
        if (doc.array().size() > kMaxBatchSize) {
            respond(QJsonDocument(jsonRpcError(
                QJsonValue::Null, -32600,
                QString("Invalid Request: a batch may hold at most %1 messages").arg(kMaxBatchSize))));
            return;
        }
        QJsonArray responses;
        for (const QJsonValue &member : doc.array()) {
            if (!member.isObject()) {
                responses.append(jsonRpcError(QJsonValue::Null, -32600,
                                              "Invalid Request: batch member is not an object"));
                continue;
            }
            const QJsonObject request = member.toObject();
            // MCP: the initialize request must not be part of a batch
            if (request.value("method").toString() == QLatin1String("initialize")) {
                // A notification gets no response, not even an error
                const QJsonValue id = request.value("id");
                if (!id.isUndefined() && !id.isNull())
                    responses.append(jsonRpcError(id, -32600,
                                                  "Invalid Request: initialize cannot be batched"));
                continue;
            }
            const QJsonObject r = handleJsonRpc(request);
            // A tool call may run a nested event loop during which the
            // client disconnects or the server is stopped; no further
            // member may run then, as for a pipelined request
            if (socket.isNull()) {
                MCP_LOG("Client disconnected before response could be sent");
                return;
            }
            if (!m_server->isListening() || socket->state() != QAbstractSocket::ConnectedState) {
                socket->abort();
                return;
            }
            if (!r.isEmpty())
                responses.append(r);
        }
        respond(responses.isEmpty() ? QJsonDocument() : QJsonDocument(responses));
        return;
    }

    // Valid JSON that is neither a request object nor a non-empty batch
    if (!doc.isObject()) {
        respond(QJsonDocument(jsonRpcError(
            QJsonValue::Null, -32600,
            "Invalid Request: expected a JSON-RPC request object or a non-empty batch array")));
        return;
    }

    QJsonObject rpcResponse = handleJsonRpc(doc.object());

    // The client may have disconnected while a tool call was running
    // inside a nested event loop (e.g. run_script, create_session).
    if (socket.isNull()) {
        MCP_LOG("Client disconnected before response could be sent");
        return;
    }

    // Only send body if there is a response (notifications have no response)
    respond(rpcResponse.isEmpty() ? QJsonDocument() : QJsonDocument(rpcResponse));
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
