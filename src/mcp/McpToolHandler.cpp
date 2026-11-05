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

#include "McpToolHandler.h"
#include "McpSessionRegistry.h"
#include "mcp_validation.h"
#include "script_escape.h"
#include "script_timeout.h"
#include "session/manager.h"
#include "agent/agent_script_runner.h"
#include "agent/tool_definitions.h"
#include "core/ebcdic.h"
#include "logger/logger.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QDir>
#include <climits>
#include <cmath>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPixmap>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUuid>

namespace mcp {

#define MCP_LOG(msg) logger::Logger::instance()->debug_with_prefix("[MCP]", msg)
#define MCP_ERROR(msg) logger::Logger::instance()->error_with_prefix("[MCP]", msg)

McpToolHandler::McpToolHandler(McpSessionRegistry *registry, QObject *parent)
    : QObject(parent), m_registry(registry) {
    connect(m_registry, &McpSessionRegistry::sessionRemoved, this,
            [this](const QString &sessionId) { m_savedCredentials.remove(sessionId); });
}

QList<session::SessionConfig> McpToolHandler::mcpAllowedSavedSessions() const {
    QList<session::SessionConfig> all;
    if (m_savedSessionsProvider) {
        all = m_savedSessionsProvider();
    } else {
        session::SessionManager manager;
        for (const QString &name : manager.listSessions()) {
            session::SessionConfig config;
            if (manager.loadSession(name, config))
                all.append(config);
        }
    }
    QList<session::SessionConfig> allowed;
    for (const auto &config : all) {
        if (config.mcpAllowed())
            allowed.append(config);
    }
    return allowed;
}

void McpToolHandler::onSessionCreated(const QString &sessionId,
                                      ui::widgets::Q5250ScreenWidget *widget) {
    m_registry->setDisplayWidget(sessionId, widget);
    if (sessionId == m_pendingSessionId) {
        m_sessionCreated = true;
    }
}

QJsonObject McpToolHandler::makeResult(const QString &text, bool isError) const {
    QJsonObject content;
    content["type"] = "text";
    content["text"] = text;

    QJsonObject result;
    result["content"] = QJsonArray{content};
    result["isError"] = isError;
    return result;
}

// ---------------------------------------------------------------------------
// Tool list
// ---------------------------------------------------------------------------

/// Names of tools that require a session_id parameter.
static bool isSessionAwareTool(const QString &name) {
    return name == agent::kToolReadScreen
        || name == agent::kToolReadLine
        || name == agent::kToolReadRegion
        || name == agent::kToolGetCursorPosition
        || name == agent::kToolGetScreenSize
        || name == agent::kToolGetFieldAt
        || name == agent::kToolFindText
        || name == agent::kToolSendKeys
        || name == agent::kToolPressKey
        || name == agent::kToolPressKeys
        || name == agent::kToolTypeText
        || name == agent::kToolSetCursorPosition
        || name == agent::kToolMoveCursor
        || name == agent::kToolWaitForText
        || name == agent::kToolRunScript
        || name == agent::kToolLogin
        || name == agent::kToolClearInputs;
}

QJsonArray McpToolHandler::listTools() const {
    struct ToolDef {
        QString name;
        QString description;
        QJsonObject (*schemaFn)();
    };

    // Session-aware tools from agent definitions
    const ToolDef agentTools[] = {
        {agent::kToolClearInputs, agent::kToolClearInputsDescription, agent::toolClearInputsSchema},
        {agent::kToolFindText, agent::kToolFindTextDescription, agent::toolFindTextSchema},
        {agent::kToolGetCursorPosition, agent::kToolGetCursorPositionDescription, agent::toolGetCursorPositionSchema},
        {agent::kToolGetFieldAt, agent::kToolGetFieldAtDescription, agent::toolGetFieldAtSchema},
        {agent::kToolGetScreenSize, agent::kToolGetScreenSizeDescription, agent::toolGetScreenSizeSchema},
        {agent::kToolListFiles, agent::kToolListFilesDescription, agent::toolListFilesSchema},
        {agent::kToolLogin, agent::kToolLoginDescription, agent::toolLoginSchema},
        {agent::kToolMoveCursor, agent::kToolMoveCursorDescription, agent::toolMoveCursorSchema},
        {agent::kToolPressKey, agent::kToolPressKeyDescription, agent::toolPressKeySchema},
        {agent::kToolPressKeys, agent::kToolPressKeysDescription, agent::toolPressKeysSchema},
        {agent::kToolReadFile, agent::kToolReadFileDescription, agent::toolReadFileSchema},
        {agent::kToolReadLine, agent::kToolReadLineDescription, agent::toolReadLineSchema},
        {agent::kToolReadRegion, agent::kToolReadRegionDescription, agent::toolReadRegionSchema},
        {agent::kToolReadScreen, agent::kToolReadScreenDescription, agent::toolReadScreenSchema},
        {agent::kToolRunScript, agent::kToolRunScriptDescription, agent::toolRunScriptSchema},
        {agent::kToolSendKeys, agent::kToolSendKeysDescription, agent::toolSendKeysSchema},
        {agent::kToolSetCursorPosition, agent::kToolSetCursorPositionDescription, agent::toolSetCursorPositionSchema},
        {agent::kToolTypeText, agent::kToolTypeTextDescription, agent::toolTypeTextSchema},
        {agent::kToolWaitForText, agent::kToolWaitForTextDescription, agent::toolWaitForTextSchema},
        {agent::kToolWriteFile, agent::kToolWriteFileDescription, agent::toolWriteFileSchema},
    };

    // MCP-only session lifecycle tools
    const ToolDef mcpTools[] = {
        {agent::kToolCreateSession, agent::kToolCreateSessionDescription, agent::toolCreateSessionSchema},
        {agent::kToolCloseSession, agent::kToolCloseSessionDescription, agent::toolCloseSessionSchema},
        {agent::kToolListSavedSessions, agent::kToolListSavedSessionsDescription, agent::toolListSavedSessionsSchema},
        {agent::kToolListSessions, agent::kToolListSessionsDescription, agent::toolListSessionsSchema},
        {agent::kToolScreenshot, agent::kToolScreenshotDescription, agent::toolScreenshotSchema},
        {agent::kToolSetDebugLogging, agent::kToolSetDebugLoggingDescription, agent::toolSetDebugLoggingSchema},
        {agent::kToolGetLogs, agent::kToolGetLogsDescription, agent::toolGetLogsSchema},
    };

    // Build session_id property once
    QJsonObject sidProp;
    sidProp["type"] = "string";
    sidProp["description"] = "Session ID returned by create_session";

    QJsonArray arr;

    // Add MCP-only tools first
    for (const auto &t : mcpTools) {
        QJsonObject tool;
        tool["name"] = t.name;
        tool["description"] = t.description;
        tool["inputSchema"] = t.schemaFn();
        arr.append(tool);
    }

    // Add agent tools, injecting session_id for session-aware ones
    for (const auto &t : agentTools) {
        QJsonObject schema = t.schemaFn();
        if (isSessionAwareTool(t.name)) {
            QJsonObject props = schema["properties"].toObject();
            props["session_id"] = sidProp;
            schema["properties"] = props;
            QJsonArray req = schema["required"].toArray();
            req.append("session_id");
            schema["required"] = req;
        }
        QJsonObject tool;
        tool["name"] = t.name;
        tool["description"] = t.description;
        tool["inputSchema"] = schema;
        arr.append(tool);
    }

    return arr;
}

// ---------------------------------------------------------------------------
// Tool dispatch
// ---------------------------------------------------------------------------

QJsonObject McpToolHandler::callTool(const QString &name, const QJsonObject &arguments) {
    MCP_LOG(QString("Tool call: %1").arg(name));

    // Per-session busy guard: only reject calls that target a session
    // currently inside a nested event loop (script execution, session
    // creation).  Calls to other sessions, or session-independent tools,
    // proceed freely.  This prevents use-after-free (e.g. close_session
    // deleting a widget while a script is using it) while allowing
    // parallel work across different sessions.
    QString sessionId = arguments.value("session_id").toString();
    if (!sessionId.isEmpty() && m_busySessions.contains(sessionId)) {
        MCP_LOG(QString("Rejected call to '%1' — session %2 is busy").arg(name, sessionId));
        return makeResult("Session is busy processing another tool call. Try again.", true);
    }

    // Session lifecycle
    if (name == agent::kToolCreateSession)    return handleCreateSession(arguments);
    if (name == agent::kToolCloseSession)     return handleCloseSession(arguments);
    if (name == agent::kToolListSavedSessions) return handleListSavedSessions();
    if (name == agent::kToolListSessions)     return handleListSessions();
    if (name == agent::kToolScreenshot)       return handleScreenshot(arguments);

    // Session-aware tools (read-only)
    if (name == agent::kToolReadScreen)       return handleReadScreen(arguments);
    if (name == agent::kToolReadLine)         return handleReadLine(arguments);
    if (name == agent::kToolReadRegion)       return handleReadRegion(arguments);
    if (name == agent::kToolGetCursorPosition) return handleGetCursorPosition(arguments);
    if (name == agent::kToolGetScreenSize)    return handleGetScreenSize(arguments);
    if (name == agent::kToolGetFieldAt)       return handleGetFieldAt(arguments);
    if (name == agent::kToolFindText)         return handleFindText(arguments);

    // Session-aware tools (actions)
    if (name == agent::kToolSendKeys)         return handleSendKeys(arguments);
    if (name == agent::kToolPressKey)         return handlePressKey(arguments);
    if (name == agent::kToolPressKeys)        return handlePressKeys(arguments);
    if (name == agent::kToolTypeText)         return handleTypeText(arguments);
    if (name == agent::kToolSetCursorPosition) return handleSetCursorPosition(arguments);
    if (name == agent::kToolMoveCursor)       return handleMoveCursor(arguments);
    if (name == agent::kToolWaitForText)      return handleWaitForText(arguments);
    if (name == agent::kToolRunScript)        return handleRunScript(arguments);
    if (name == agent::kToolLogin)            return handleLogin(arguments);
    if (name == agent::kToolClearInputs)     return handleClearInputs(arguments);

    // Session-independent
    if (name == agent::kToolListFiles)        return handleListFiles(arguments);
    if (name == agent::kToolReadFile)         return handleReadFile(arguments);
    if (name == agent::kToolWriteFile)        return handleWriteFile(arguments);
    if (name == agent::kToolSetDebugLogging)  return handleSetDebugLogging(arguments);
    if (name == agent::kToolGetLogs)          return handleGetLogs(arguments);

    // McpServer answers names outside listTools() with a -32602 error
    // before calling here; this covers a direct caller
    return makeResult(unknownToolMessage(name), true);
}

bool McpToolHandler::hasTool(const QString &name) const {
    for (const QJsonValue &tool : listTools()) {
        if (tool.toObject().value("name").toString() == name)
            return true;
    }
    return false;
}

QString McpToolHandler::unknownToolMessage(const QString &name) {
    if (name.isEmpty())
        return QStringLiteral("No tool name provided.");
    if (name == agent::kToolConnect)
        return QStringLiteral("The 'connect' tool has been replaced by 'create_session'.");
    if (name == agent::kToolGenerateScript)
        return QStringLiteral("generate_5250script is not available over MCP.");
    return QStringLiteral("Unknown tool: ") + name;
}

// ---------------------------------------------------------------------------
// Session resolution helper
// ---------------------------------------------------------------------------

ui::widgets::Q5250ScreenWidget *McpToolHandler::resolveSession(
    const QJsonObject &args, QJsonObject &errorResult)
{
    QString sessionId = args.value("session_id").toString();
    if (sessionId.isEmpty()) {
        errorResult = makeResult("Missing required parameter: session_id", true);
        return nullptr;
    }
    // Returns a snapshot by value: the QPointer inside tracks widget
    // destruction, and holding a copy means a later registry mutation
    // cannot invalidate what we read here.
    McpSessionInfo info = m_registry->session(sessionId);
    if (info.sessionId.isEmpty() || !info.displayWidget) {
        MCP_ERROR(QString("Session not found: %1").arg(sessionId));
        errorResult = makeResult("Session not found or has been closed: " + sessionId, true);
        return nullptr;
    }
    return info.displayWidget;
}

// ---------------------------------------------------------------------------
// Session lifecycle tools
// ---------------------------------------------------------------------------

QJsonObject McpToolHandler::handleCreateSession(const QJsonObject &args) {
    QString hostname = args.value("hostname").toString();
    QString profile = args.value("profile").toString();
    if (!hostname.isEmpty() && !profile.isEmpty())
        return makeResult("Pass either hostname or profile, not both.", true);
    if (hostname.isEmpty() && profile.isEmpty())
        return makeResult("No hostname provided.", true);

    session::SessionConfig config;
    if (!profile.isEmpty()) {
        bool found = false;
        for (const auto &saved : mcpAllowedSavedSessions()) {
            if (saved.name() == profile) {
                config = saved;
                found = true;
                break;
            }
        }
        if (!found)
            return makeResult(QString("No saved session named '%1' is available to MCP "
                                      "clients. Use list_saved_sessions.").arg(profile), true);
        hostname = config.hostname();
    } else {
        quint16 port = 0;
        if (!parseTcpPort(args.value("port"), &port)) {
            return makeResult("Port must be an integer between 1 and 65535.", true);
        }
        QString deviceType = args.value("deviceType").toString("IBM-3179-2").trimmed();
        if (deviceType.isEmpty())
            return makeResult("Device type must not be empty.", true);
        config.setHostname(hostname);
        config.setPort(port);
        config.setUseTLS(args.value("useTLS").toBool(false));
        config.setDeviceType(deviceType);
    }
    quint16 port = config.port();
    bool useTLS = config.useTLS();
    QString deviceType = config.deviceType();

    MCP_LOG(QString("Creating session to %1:%2 (TLS=%3, device=%4)")
                .arg(hostname).arg(port).arg(useTLS).arg(deviceType));

    // Generate session ID
    QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Register session (widget will be set later via onSessionCreated)
    McpSessionInfo info;
    info.sessionId = sessionId;
    info.hostname = hostname;
    info.port = port;
    info.useTLS = useTLS;
    m_registry->addSession(info);

    // Set up wait for session creation. Mark this session as busy to reject
    // reentrant tool calls targeting it during the event loop.
    m_busySessions.insert(sessionId);
    m_pendingSessionId = sessionId;
    m_sessionCreated = false;

    QEventLoop loop;
    QTimer::singleShot(15000, &loop, [&]() { loop.quit(); });

    // Keep a saved session's credentials for login; they stay in-process.
    if (!profile.isEmpty() && !config.password().isEmpty())
        m_savedCredentials.insert(sessionId, {config.username(), config.password()});

    // Emit the request — MainWindow creates the tab synchronously on the
    // same thread, which calls onSessionCreated() before we reach loop.exec()
    emit createSessionRequested(sessionId, config);

    if (!m_sessionCreated)
        loop.exec();

    m_pendingSessionId.clear();
    m_busySessions.remove(sessionId);

    if (!m_sessionCreated) {
        MCP_ERROR(QString("Session creation timed out for %1:%2").arg(hostname).arg(port));
        m_registry->removeSession(sessionId);
        return makeResult("Session creation timed out.", true);
    }

    MCP_LOG(QString("Session created: %1 -> %2:%3").arg(sessionId, hostname).arg(port));
    // The tab exists now, but its connection is still being opened: say
    // so instead of claiming a connection that may never succeed
    return makeResult(QString("session_id: %1\nConnecting to %2:%3. The connection is opened in "
                              "the background: poll read_screen until the host's screen "
                              "appears (it is blank until then). If list_sessions still "
                              "shows connected: no after a few seconds, the connection "
                              "failed.")
                          .arg(sessionId, hostname).arg(port));
}

QJsonObject McpToolHandler::handleCloseSession(const QJsonObject &args) {
    QString sessionId = args.value("session_id").toString();
    if (sessionId.isEmpty())
        return makeResult("Missing required parameter: session_id", true);

    if (!m_registry->hasSession(sessionId))
        return makeResult("Session not found: " + sessionId, true);

    // Refuse to close a session whose widget is currently being used by
    // another tool call inside a nested event loop (e.g. run_script,
    // create_session).  Tearing the tab down while the script is stepping
    // through the widget would destroy state that the nested loop still
    // references, leading to use-after-free even with our QPointer guards
    // (ScriptExecutor can call back into the adapter between the destruction
    // and the nested loop's next iteration).  The client should wait for the
    // in-flight call to return, then retry close_session.
    if (m_busySessions.contains(sessionId)) {
        MCP_ERROR(QString("Refusing to close busy session: %1").arg(sessionId));
        return makeResult(
            "Session is busy (a script or command is in progress); "
            "wait for it to finish, then retry.", true);
    }

    MCP_LOG(QString("Closing session: %1").arg(sessionId));
    // Remove from registry first to prevent dangling widget pointers.
    // onCloseTabRequested also calls onSessionClosed→removeSession, but
    // the second remove is a harmless no-op.
    m_registry->removeSession(sessionId);
    emit closeSessionRequested(sessionId);

    return makeResult("Session closed: " + sessionId);
}

QJsonObject McpToolHandler::handleListSavedSessions() {
    const auto sessions = mcpAllowedSavedSessions();
    if (sessions.isEmpty())
        return makeResult("No saved sessions are available to MCP clients. Enable "
                          "\"Allow MCP clients to open this session\" in a saved "
                          "session's settings.");

    QStringList lines;
    for (const auto &s : sessions) {
        lines << QString("profile: %1  host: %2:%3  tls: %4  saved credentials: %5")
                     .arg(s.name(), s.hostname())
                     .arg(s.port())
                     .arg(s.useTLS() ? "yes" : "no")
                     .arg(s.password().isEmpty() ? "no" : "yes");
    }
    return makeResult(lines.join('\n'));
}

QJsonObject McpToolHandler::handleListSessions() {
    auto sessions = m_registry->allSessions();
    if (sessions.isEmpty())
        return makeResult("No active MCP sessions.");

    QStringList lines;
    for (const auto &s : sessions) {
        lines << QString("session_id: %1  host: %2:%3  tls: %4  connected: %5")
                     .arg(s.sessionId, s.hostname)
                     .arg(s.port)
                     .arg(s.useTLS ? "yes" : "no")
                     .arg(s.connected ? "yes" : "no");
    }
    return makeResult(lines.join('\n'));
}

// ---------------------------------------------------------------------------
// Screen tools (require GUI thread access)
// ---------------------------------------------------------------------------

/// Replace NUL and non-printable characters with spaces.
static QChar sanitizeChar(QChar ch) {
    if (ch.isNull() || !ch.isPrint())
        return QLatin1Char(' ');
    return ch;
}

QString McpToolHandler::readScreenText(ui::widgets::Q5250ScreenWidget *widget) {
    if (!widget) return {};

    // MCP server runs on the main GUI thread, so we can access widgets directly.
    QString screenText;
    auto *buf = widget->screenBuffer();
    if (!buf) return {};
    for (int r = 0; r < buf->rows(); ++r) {
        QString line;
        for (int c = 0; c < buf->cols(); ++c) {
            uint8_t ch = buf->displayedCharacter(r, c);
            line += sanitizeChar(core::EBCDIC::ebcdicToChar(ch));
        }
        while (line.endsWith(' '))
            line.chop(1);
        screenText += line + '\n';
    }
    return screenText;
}

QJsonObject McpToolHandler::handleReadScreen(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString text = readScreenText(widget);
    return makeResult(text.isEmpty() ? "(no screen content)" : text);
}

QJsonObject McpToolHandler::handleGetCursorPosition(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    int row = -1, col = -1;
    auto *buf = widget->screenBuffer();
    if (buf) {
        QPoint pos = buf->cursorPosition();
        row = pos.y();
        col = pos.x();
    }

    if (row < 0)
        return makeResult("Failed to read cursor position.", true);
    return makeResult(QString("row: %1, col: %2").arg(row).arg(col));
}

QJsonObject McpToolHandler::handleGetFieldAt(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    int row = args.value("row").toInt(-1);
    int col = args.value("col").toInt(-1);

    auto *buf = widget->screenBuffer();
    if (!buf)
        return makeResult("Failed to access screen buffer.", true);
    if (row < 0 || row >= buf->rows() || col < 0 || col >= buf->cols())
        return makeResult(QString("Position (%1, %2) out of bounds (screen is %3x%4).")
            .arg(row).arg(col).arg(buf->rows()).arg(buf->cols()), true);
    if (!buf->isInField(row, col))
        return makeResult(QString("No field at position (%1, %2).").arg(row).arg(col), true);

    auto field = buf->getField(row, col);
    QByteArray data = buf->getDisplayedFieldData(field);
    QString fieldText;
    for (uint8_t byte : data)
        fieldText += sanitizeChar(core::EBCDIC::ebcdicToChar(byte));
    QString resultText = QString("startRow: %1, startCol: %2, length: %3, protected: %4, modified: %5, text: \"%6\"")
        .arg(field.startRow).arg(field.startCol).arg(field.length)
        .arg(field.protected_field ? "true" : "false")
        .arg(field.modified ? "true" : "false")
        .arg(fieldText.trimmed());
    return makeResult(resultText);
}

QJsonObject McpToolHandler::handleGetScreenSize(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    auto *buf = widget->screenBuffer();
    if (!buf)
        return makeResult("Failed to access screen buffer.", true);

    return makeResult(QString("rows: %1, cols: %2").arg(buf->rows()).arg(buf->cols()));
}

QJsonObject McpToolHandler::handleFindText(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString text = args.value("text").toString();
    if (text.isEmpty())
        return makeResult("Missing required parameter: text", true);

    auto *buf = widget->screenBuffer();
    if (!buf)
        return makeResult("Failed to access screen buffer.", true);

    // Read each row and search for the text
    QStringList matches;
    for (int r = 0; r < buf->rows(); ++r) {
        QString line;
        for (int c = 0; c < buf->cols(); ++c) {
            uint8_t ch = buf->displayedCharacter(r, c);
            line += sanitizeChar(core::EBCDIC::ebcdicToChar(ch));
        }
        int col = 0;
        while ((col = line.indexOf(text, col, Qt::CaseInsensitive)) != -1) {
            matches << QString("row: %1, col: %2").arg(r).arg(col);
            col += text.length();
        }
    }

    if (matches.isEmpty())
        return makeResult("Text not found: " + text, true);

    return makeResult(matches.join('\n'));
}

QJsonObject McpToolHandler::handleReadLine(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    int row = args.value("row").toInt(-1);
    auto *buf = widget->screenBuffer();
    if (!buf)
        return makeResult("Failed to access screen buffer.", true);
    if (row < 0 || row >= buf->rows())
        return makeResult(QString("Row %1 out of bounds (screen has %2 rows).")
            .arg(row).arg(buf->rows()), true);

    QString line;
    for (int c = 0; c < buf->cols(); ++c) {
        uint8_t ch = buf->displayedCharacter(row, c);
        line += sanitizeChar(core::EBCDIC::ebcdicToChar(ch));
    }
    while (line.endsWith(' '))
        line.chop(1);
    return makeResult(line);
}

QJsonObject McpToolHandler::handleReadRegion(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    int row = args.value("row").toInt(-1);
    int col = args.value("col").toInt(-1);
    int numRows = args.value("numRows").toInt(-1);
    int numCols = args.value("numCols").toInt(-1);

    auto *buf = widget->screenBuffer();
    if (!buf)
        return makeResult("Failed to access screen buffer.", true);
    if (row < 0 || col < 0 || numRows <= 0 || numCols <= 0)
        return makeResult("Invalid region parameters.", true);
    // Compared without adding, which could overflow for huge arguments
    if (numRows > buf->rows() - row || numCols > buf->cols() - col)
        return makeResult(QString("Region (%1,%2)+(%3,%4) exceeds screen bounds (%5x%6).")
            .arg(row).arg(col).arg(numRows).arg(numCols)
            .arg(buf->rows()).arg(buf->cols()), true);

    QString result;
    for (int r = row; r < row + numRows; ++r) {
        QString line;
        for (int c = col; c < col + numCols; ++c) {
            uint8_t ch = buf->displayedCharacter(r, c);
            line += sanitizeChar(core::EBCDIC::ebcdicToChar(ch));
        }
        while (line.endsWith(' '))
            line.chop(1);
        result += line + '\n';
    }
    return makeResult(result);
}

QJsonObject McpToolHandler::handleScreenshot(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString path = args.value("path").toString();
    if (path.isEmpty())
        return makeResult("Missing required parameter: path", true);

    MCP_LOG(QString("Taking screenshot to: %1").arg(path));

    QPixmap pixmap = widget->grab();
    if (pixmap.isNull()) {
        MCP_ERROR("Screenshot capture returned null pixmap");
        return makeResult("Failed to capture screenshot.", true);
    }

    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    if (!pixmap.save(path, "PNG")) {
        MCP_ERROR(QString("Failed to save screenshot to: %1").arg(path));
        return makeResult("Failed to save screenshot to: " + path, true);
    }

    MCP_LOG(QString("Screenshot saved: %1 (%2x%3)")
                .arg(fi.absoluteFilePath()).arg(pixmap.width()).arg(pixmap.height()));
    return makeResult("Screenshot saved: " + fi.absoluteFilePath());
}

// ---------------------------------------------------------------------------
// Filesystem tools (no GUI thread needed)
// ---------------------------------------------------------------------------

QJsonObject McpToolHandler::handleListFiles(const QJsonObject &args) {
    QString path = args.value("path").toString();
    if (path.isEmpty()) path = ".";

    QDir dir(path);
    if (!dir.exists())
        return makeResult("Directory does not exist: " + path, true);

    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name | QDir::DirsFirst);

    QStringList lines;
    for (const QFileInfo &fi : entries) {
        QString type = fi.isDir() ? "[DIR] " : "      ";
        QString size = fi.isFile() ? QString::number(fi.size()) : "-";
        lines << QString("%1 %2  %3").arg(type, -6).arg(size, 10).arg(fi.fileName());
    }
    return makeResult(lines.isEmpty() ? "(empty directory)" : lines.join('\n'));
}

QJsonObject McpToolHandler::handleReadFile(const QJsonObject &args) {
    QString path = args.value("path").toString();
    if (path.isEmpty())
        return makeResult("No file path provided.", true);

    // Open without QIODevice::Text so bytes are returned verbatim; the Text
    // flag translates CRLF/CR to LF on read, which corrupts binary files and
    // makes a read/write round-trip lossy.
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return makeResult("Failed to read file: " + file.errorString(), true);

    QString content = QString::fromUtf8(file.readAll());
    file.close();
    return makeResult(content);
}

// ---------------------------------------------------------------------------
// Diagnostics tools
// ---------------------------------------------------------------------------

QJsonObject McpToolHandler::handleSetDebugLogging(const QJsonObject &args) {
    const QJsonValue enabled = args.value("enabled");
    if (!enabled.isBool())
        return makeResult("Missing or invalid parameter: enabled (expected true or false)", true);

    auto *log = logger::Logger::instance();
    log->setDebugLogging(enabled.toBool());
    if (!log->isDebugLogging())
        return makeResult("Debug logging disabled.");
    const QString path = log->logFilePath();
    return makeResult(QString("Debug logging enabled. Log file: %1")
                          .arg(path.isEmpty() ? QStringLiteral("none (could not be opened)") : path));
}

QJsonObject McpToolHandler::handleGetLogs(const QJsonObject &args) {
    const QJsonValue maxLinesValue = args.value("max_lines");
    int maxLines = 1000;
    if (!maxLinesValue.isUndefined()) {
        const double number = maxLinesValue.toDouble(-1);
        if (!maxLinesValue.isDouble() || std::trunc(number) != number || number < 0)
            return makeResult("Invalid parameter: max_lines (expected a non-negative integer)", true);
        maxLines = number > INT_MAX ? INT_MAX : static_cast<int>(number);
    }
    const QString filter = args.value("filter").toString();

    auto *log = logger::Logger::instance();
    QStringList lines = log->recentLogs();
    if (!filter.isEmpty())
        lines = lines.filter(filter, Qt::CaseInsensitive);
    if (maxLines > 0 && lines.size() > maxLines)
        lines = lines.mid(lines.size() - maxLines);

    QString text = log->diagnosticHeader();
    if (lines.isEmpty())
        text += filter.isEmpty() ? "(no log lines)" : "(no log lines match the filter)";
    else
        text += lines.join('\n');
    return makeResult(text);
}

QJsonObject McpToolHandler::handleWriteFile(const QJsonObject &args) {
    QString path = args.value("path").toString();
    if (path.isEmpty())
        return makeResult("No file path provided.", true);
    // Check before opening: opening with Truncate would empty an existing
    // file even though nothing is written to it.
    if (!args.value("content").isString())
        return makeResult("Missing required parameter: content", true);
    const QString content = args.value("content").toString();

    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    // Open without QIODevice::Text so the payload is written verbatim;
    // otherwise Qt injects platform-specific line-ending translations on
    // Windows and the bytes on disk differ from what the caller supplied.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return makeResult("Failed to write file: " + file.errorString(), true);

    const QByteArray payload = content.toUtf8();
    const qint64 written = file.write(payload);
    if (written != payload.size()) {
        const QString errText = file.errorString();
        file.close();
        return makeResult(QString("Failed to write file (wrote %1 of %2 bytes): %3")
                              .arg(written).arg(payload.size()).arg(errText),
                          true);
    }
    if (!file.flush()) {
        const QString errText = file.errorString();
        file.close();
        return makeResult("Failed to flush file: " + errText, true);
    }
    file.close();
    if (file.error() != QFileDevice::NoError)
        return makeResult("Failed to close file: " + file.errorString(), true);
    return makeResult("File written successfully: " + path);
}

// ---------------------------------------------------------------------------
// Script tools (require GUI thread for script execution)
// ---------------------------------------------------------------------------

QJsonObject McpToolHandler::handleRunScript(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString script = args.value("script").toString();
    if (script.isEmpty())
        return makeResult("No script provided.", true);

    MCP_LOG(QString("Running script (%1 chars) on session %2")
                .arg(script.length()).arg(args.value("session_id").toString()));

    // Run script with a local event loop. Mark this session as busy to
    // reject reentrant tool calls that could delete the widget while the
    // script runs.  Other sessions remain accessible.
    QString sessionId = args.value("session_id").toString();
    m_busySessions.insert(sessionId);

    bool success = false;
    QString log;
    bool done = false;

    // Use QPointer to detect widget destruction during execution.
    QPointer<ui::widgets::Q5250ScreenWidget> widgetGuard = widget;

    int timeoutMs = scriptTimeoutMs(script);

    auto *runner = new agent::AgentScriptRunner(widget, this);
    QEventLoop loop;
    QObject::connect(runner, &agent::AgentScriptRunner::finished,
                     &loop, [&](bool ok, const QString &output) {
        if (done) return;
        success = ok;
        log = output;
        done = true;
        // Use a zero-timer to exit the loop after the current signal chain
        // finishes, avoiding use-after-free in the script executor's cleanup.
        QTimer::singleShot(0, &loop, &QEventLoop::quit);
    });
    QTimer::singleShot(timeoutMs, &loop, [&]() {
        if (done) return;
        log = QString("Script execution timed out after %1 seconds.")
                  .arg(timeoutMs / 1000);
        done = true;
        if (runner->isRunning()) runner->stop();
        QTimer::singleShot(0, &loop, &QEventLoop::quit);
    });

    runner->runScript(script);
    if (!done)
        loop.exec();

    // Stop if still running (e.g. timeout).
    if (runner->isRunning())
        runner->stop();
    // Use deleteLater — the runner's cleanup() defers executor/adapter
    // deletion to the next event cycle, so synchronous delete would
    // destroy the runner before those deferred deletions fire.
    runner->deleteLater();

    m_busySessions.remove(sessionId);

    if (widgetGuard.isNull()) {
        MCP_ERROR("Widget destroyed during script execution");
        return makeResult("Terminal session was closed during script execution.", true);
    }

    MCP_LOG(QString("Script finished: %1").arg(success ? "OK" : "FAILED"));
    return makeResult(log.isEmpty() ? "(script completed)" : log, !success);
}

QJsonObject McpToolHandler::handleSendKeys(const QJsonObject &args) {
    // Validate session_id first
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString keys = args.value("keys").toString();
    if (keys.isEmpty())
        return makeResult("No keys provided.", true);

    // Convert key names to a minimal 5250script
    // Arrow key names map to MOVE CURSOR commands, not PRESS.
    static const QSet<QString> arrowKeys = {"UP", "DOWN", "LEFT", "RIGHT"};

    QStringList scriptLines;
    QString firstKey;
    QRegularExpression tokenRe("\"([^\"]*)\"|\\S+");
    auto it = tokenRe.globalMatch(keys);
    while (it.hasNext()) {
        auto match = it.next();
        if (scriptLines.isEmpty() && match.captured(1).isNull())
            firstKey = match.captured(0).toUpper();
        if (!match.captured(1).isNull()) {
            QString text = escapeInterpolatedScriptString(match.captured(1));
            scriptLines << QString("TYPE \"%1\"").arg(text);
        } else {
            QString key = match.captured(0).toUpper();
            if (arrowKeys.contains(key))
                scriptLines << QString("MOVE CURSOR %1").arg(key);
            else
                scriptLines << QString("PRESS %1").arg(key);
        }
    }

    QString lockErr = lockedKeyboardError(widget->keyboardState(), firstKey);
    if (!lockErr.isEmpty())
        return makeResult(lockErr, true);

    // Build args with session_id preserved
    QJsonObject scriptArgs;
    scriptArgs["script"] = scriptLines.join('\n');
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

/// Valid key names for press_key / press_keys.
static const QSet<QString> &validKeyNames() {
    static const QSet<QString> keys = {
        // AID keys
        "ENTER", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9",
        "F10", "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18",
        "F19", "F20", "F21", "F22", "F23", "F24",
        "PAGEUP", "PAGEDOWN", "ATTN", "SYSREQ", "HELP", "CLEAR", "PRINT",
        // Local keys
        "TAB", "BACKTAB", "BACKSPACE", "DELETE", "INSERT", "HOME", "END",
        "ESC", "ESCAPE",
        "FIELDPLUS", "FIELDMINUS", "FIELDEXIT", "DUP",
        "ERASEINPUT", "ERASEFIELD", "ERASEEOF",
        // Cursor movement
        "UP", "DOWN", "LEFT", "RIGHT",
    };
    return keys;
}

/// Arrow keys that map to MOVE CURSOR instead of PRESS.
static const QSet<QString> &arrowKeyNames() {
    static const QSet<QString> keys = {"UP", "DOWN", "LEFT", "RIGHT"};
    return keys;
}

/// Convert a validated key name to a 5250script line.
static QString keyToScriptLine(const QString &key) {
    if (arrowKeyNames().contains(key))
        return QStringLiteral("MOVE CURSOR ") + key;
    return QStringLiteral("PRESS ") + key;
}

QJsonObject McpToolHandler::handlePressKey(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString key = args.value("key").toString().trimmed().toUpper();
    if (key.isEmpty())
        return makeResult("Missing required parameter: key", true);
    if (!validKeyNames().contains(key))
        return makeResult("Unknown key: " + key + ". Valid keys: " +
                          QStringList(validKeyNames().values()).join(", "), true);

    QString lockErr = lockedKeyboardError(widget->keyboardState(), key);
    if (!lockErr.isEmpty())
        return makeResult(lockErr, true);

    QJsonObject scriptArgs;
    scriptArgs["script"] = keyToScriptLine(key);
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

QJsonObject McpToolHandler::handlePressKeys(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QJsonArray keysArray = args.value("keys").toArray();
    if (keysArray.isEmpty())
        return makeResult("Missing or empty required parameter: keys", true);

    QStringList scriptLines;
    for (const QJsonValue &v : keysArray) {
        QString key = v.toString().trimmed().toUpper();
        if (key.isEmpty())
            return makeResult("Empty key name in array.", true);
        if (!validKeyNames().contains(key))
            return makeResult("Unknown key: " + key + ". Valid keys: " +
                              QStringList(validKeyNames().values()).join(", "), true);
        scriptLines << keyToScriptLine(key);
    }

    QString lockErr = lockedKeyboardError(widget->keyboardState(),
                                          keysArray.first().toString().trimmed().toUpper());
    if (!lockErr.isEmpty())
        return makeResult(lockErr, true);

    QJsonObject scriptArgs;
    scriptArgs["script"] = scriptLines.join('\n');
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

QJsonObject McpToolHandler::handleTypeText(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString text = args.value("text").toString();
    if (text.isEmpty())
        return makeResult("Missing required parameter: text", true);

    QString lockErr = lockedKeyboardError(widget->keyboardState(), QString());
    if (!lockErr.isEmpty())
        return makeResult(lockErr, true);

    // Escape for the 5250script TYPE command (quotes, backslashes, newlines)
    QString safeText = escapeInterpolatedScriptString(text);

    QJsonObject scriptArgs;
    scriptArgs["script"] = QString("TYPE \"%1\"").arg(safeText);
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

QJsonObject McpToolHandler::handleSetCursorPosition(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    int row = args.value("row").toInt(-1);
    int col = args.value("col").toInt(-1);

    auto *buf = widget->screenBuffer();
    if (!buf)
        return makeResult("Failed to access screen buffer.", true);
    if (row < 0 || row >= buf->rows() || col < 0 || col >= buf->cols())
        return makeResult(QString("Position (%1, %2) out of bounds (screen is %3x%4).")
            .arg(row).arg(col).arg(buf->rows()).arg(buf->cols()), true);

    // MOVE CURSOR AT uses 1-based coordinates in 5250script
    QJsonObject scriptArgs;
    scriptArgs["script"] = QString("MOVE CURSOR AT (%1,%2)").arg(row + 1).arg(col + 1);
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

QJsonObject McpToolHandler::handleMoveCursor(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    int rows = args.value("rows").toInt(0);
    int cols = args.value("cols").toInt(0);

    if (rows == 0 && cols == 0)
        return makeResult("No movement specified.", true);

    QStringList scriptLines;
    if (rows > 0) {
        for (int i = 0; i < rows; ++i)
            scriptLines << "MOVE CURSOR DOWN";
    } else if (rows < 0) {
        for (int i = 0; i < -rows; ++i)
            scriptLines << "MOVE CURSOR UP";
    }
    if (cols > 0) {
        for (int i = 0; i < cols; ++i)
            scriptLines << "MOVE CURSOR RIGHT";
    } else if (cols < 0) {
        for (int i = 0; i < -cols; ++i)
            scriptLines << "MOVE CURSOR LEFT";
    }

    QJsonObject scriptArgs;
    scriptArgs["script"] = scriptLines.join('\n');
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

QJsonObject McpToolHandler::handleWaitForText(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString text = args.value("text").toString();
    if (text.isEmpty())
        return makeResult("Missing required parameter: text", true);

    int timeout = args.value("timeout").toInt(30000);

    // Escape for 5250script (quotes, backslashes, newlines). EXPECT TEXT
    // matches its literal verbatim, but the ABORT message is interpolated.
    QString safeText = escapeScriptString(text);
    QString safeMessage = escapeInterpolatedScriptString(text);

    QString script = QString(
        "GLOBAL EXPECT_TIMEOUT %1\n"
        "EXPECT TEXT \"%2\"\n"
        "IF $EXPECT_RESULT == \"TIMEOUT\" THEN\n"
        "    ABORT \"Timed out waiting for text: %3\"\n"
        "ENDIF\n"
    ).arg(timeout).arg(safeText, safeMessage);

    QJsonObject scriptArgs;
    scriptArgs["script"] = script;
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

QJsonObject McpToolHandler::handleClearInputs(const QJsonObject &args) {
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString lockErr = lockedKeyboardError(widget->keyboardState(), "ERASEINPUT");
    if (!lockErr.isEmpty())
        return makeResult(lockErr, true);

    QJsonObject scriptArgs;
    scriptArgs["script"] = QStringLiteral("PRESS ERASEINPUT");
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

QJsonObject McpToolHandler::handleLogin(const QJsonObject &args) {
    // Validate session_id first
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    // A session opened from a saved session signs on with its saved
    // credentials when the client names no other user and no password.
    QString username = args.value("username").toString();
    auto saved = m_savedCredentials.constFind(args.value("session_id").toString());
    bool useSaved = saved != m_savedCredentials.constEnd() && !args.contains("password")
        && (username.isEmpty() || username.compare(saved->username, Qt::CaseInsensitive) == 0);
    if (useSaved)
        username = saved->username;
    if (username.isEmpty())
        return makeResult("No username provided.", true);

    // Without a password argument, ask the user in the GUI so the password
    // never passes through the MCP client.
    QString password;
    if (useSaved) {
        password = saved->password;
    } else if (args.contains("password")) {
        password = args.value("password").toString();
    } else {
        if (!m_passwordPrompt)
            return makeResult("No password provided and no password prompt is available.", true);
        QString sessionId = args.value("session_id").toString();
        QPointer<ui::widgets::Q5250ScreenWidget> widgetGuard = widget;
        // The prompt runs a nested event loop: reject other calls to this
        // session until the user answers.
        m_busySessions.insert(sessionId);
        bool answered = m_passwordPrompt(sessionId, m_registry->session(sessionId).hostname,
                                         username, &password);
        m_busySessions.remove(sessionId);
        if (widgetGuard.isNull())
            return makeResult("Terminal session was closed while waiting for the password.", true);
        if (!answered)
            return makeResult("Sign-on cancelled: the user did not enter a password.", true);
    }

    // Escape credentials for 5250script to prevent script injection
    // (quotes, backslashes, and newlines — a newline would otherwise inject
    // new script statements, a trailing backslash would unterminate the
    // string literal).
    QString safeUser = escapeInterpolatedScriptString(username);
    QString safePass = escapeInterpolatedScriptString(password);

    // Login script with auto-signoff and display program messages handling.
    // Sets $SESSION_USERNAME and $SESSION_PASSWORD then calls the login flow.
    QString script = QString(
        "GLOBAL EXPECT_TIMEOUT 30000\n"
        "\n"
        "DEF login($username, $password)\n"
        "    EXPECT TEXT \"Sign On\" AT ROW 1\n"
        "    IF $EXPECT_RESULT == \"TIMEOUT\" THEN\n"
        "        LOG \"Timed out waiting for Sign On screen\"\n"
        "        ABORT \"Sign On screen not found\"\n"
        "    ENDIF\n"
        "    EXPECT KEYBOARD UNLOCKED\n"
        "    IF $EXPECT_RESULT == \"TIMEOUT\" THEN\n"
        "        ABORT \"Keyboard not ready on Sign On screen\"\n"
        "    ENDIF\n"
        "    EXTRACT $title LINE 1\n"
        "    IF $title CONTAINS \"Attempt to Recover Interactive Job\" THEN\n"
        "        autosignoff($username, $password)\n"
        "    ENDIF\n"
        "    MOVE CURSOR AT (1,1)\n"
        "    MOVE CURSOR AT NEXT INPUTFIELD\n"
        "    TYPE \"$username\"\n"
        "    MOVE CURSOR AT NEXT INPUTFIELD\n"
        "    TYPE \"$password\"\n"
        "    PRESS ENTER\n"
        "    display_program_messages()\n"
        "ENDDEF\n"
        "\n"
        "DEF autosignoff($username, $password)\n"
        "    EXPECT KEYBOARD UNLOCKED\n"
        "    IF $EXPECT_RESULT == \"TIMEOUT\" THEN\n"
        "        ABORT \"Keyboard not ready on recovery screen\"\n"
        "    ENDIF\n"
        "    EXTRACT $title LINE 1\n"
        "    IF $title CONTAINS \"Attempt to Recover Interactive Job\" THEN\n"
        "        MOVE CURSOR AT (1,1)\n"
        "        MOVE CURSOR AT NEXT INPUTFIELD\n"
        "        TYPE \"90\"\n"
        "        PRESS ENTER\n"
        "        PRESS ENTER\n"
        "        login($username, $password)\n"
        "    ENDIF\n"
        "ENDDEF\n"
        "\n"
        "DEF display_program_messages()\n"
        "    EXPECT KEYBOARD UNLOCKED\n"
        "    IF $EXPECT_RESULT == \"TIMEOUT\" THEN\n"
        "        ABORT \"Keyboard not ready on program messages screen\"\n"
        "    ENDIF\n"
        "    EXTRACT $title LINE 1\n"
        "    IF $title CONTAINS \"Display Program Messages\" THEN\n"
        "        MOVE CURSOR AT (1,1)\n"
        "        MOVE CURSOR AT NEXT INPUTFIELD\n"
        "        PRESS ENTER\n"
        "    ENDIF\n"
        "ENDDEF\n"
        "\n"
        "login(\"%1\", \"%2\")\n"
    ).arg(safeUser, safePass);

    QJsonObject scriptArgs;
    scriptArgs["script"] = script;
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

} // namespace mcp
