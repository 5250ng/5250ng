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
#include "agent/agent_script_runner.h"
#include "agent/tool_definitions.h"
#include "core/ebcdic.h"
#include "logger/logger.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPixmap>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>

namespace mcp {

#define MCP_LOG(msg) logger::Logger::instance()->debug_with_prefix("[MCP]", msg)
#define MCP_ERROR(msg) logger::Logger::instance()->error_with_prefix("[MCP]", msg)

McpToolHandler::McpToolHandler(McpSessionRegistry *registry, QObject *parent)
    : QObject(parent), m_registry(registry) {}

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
        || name == agent::kToolGetCursorPosition
        || name == agent::kToolGetFieldAt
        || name == agent::kToolSendKeys
        || name == agent::kToolRunScript
        || name == agent::kToolLogin;
}

QJsonArray McpToolHandler::listTools() const {
    struct ToolDef {
        QString name;
        QString description;
        QJsonObject (*schemaFn)();
    };

    // Session-aware tools from agent definitions
    const ToolDef agentTools[] = {
        {agent::kToolGetCursorPosition, agent::kToolGetCursorPositionDescription, agent::toolGetCursorPositionSchema},
        {agent::kToolGetFieldAt, agent::kToolGetFieldAtDescription, agent::toolGetFieldAtSchema},
        {agent::kToolListFiles, agent::kToolListFilesDescription, agent::toolListFilesSchema},
        {agent::kToolLogin, agent::kToolLoginDescription, agent::toolLoginSchema},
        {agent::kToolReadFile, agent::kToolReadFileDescription, agent::toolReadFileSchema},
        {agent::kToolReadScreen, agent::kToolReadScreenDescription, agent::toolReadScreenSchema},
        {agent::kToolRunScript, agent::kToolRunScriptDescription, agent::toolRunScriptSchema},
        {agent::kToolSendKeys, agent::kToolSendKeysDescription, agent::toolSendKeysSchema},
        {agent::kToolWriteFile, agent::kToolWriteFileDescription, agent::toolWriteFileSchema},
    };

    // MCP-only session lifecycle tools
    const ToolDef mcpTools[] = {
        {agent::kToolCreateSession, agent::kToolCreateSessionDescription, agent::toolCreateSessionSchema},
        {agent::kToolCloseSession, agent::kToolCloseSessionDescription, agent::toolCloseSessionSchema},
        {agent::kToolListSessions, agent::kToolListSessionsDescription, agent::toolListSessionsSchema},
        {agent::kToolScreenshot, agent::kToolScreenshotDescription, agent::toolScreenshotSchema},
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
    // Guard: reject reentrant calls that arrive during a nested event loop
    // (e.g. handleRunScript's loop.exec() processes new TCP data).
    // Without this, a close_session during script execution could delete the
    // widget that the running script is actively using → use-after-free.
    if (m_busy) {
        MCP_LOG(QString("Rejected reentrant call to '%1' (busy)").arg(name));
        return makeResult("Server is busy processing another tool call. Try again.", true);
    }

    MCP_LOG(QString("Tool call: %1").arg(name));

    // Session lifecycle
    if (name == agent::kToolCreateSession)    return handleCreateSession(arguments);
    if (name == agent::kToolCloseSession)     return handleCloseSession(arguments);
    if (name == agent::kToolListSessions)     return handleListSessions();
    if (name == agent::kToolScreenshot)       return handleScreenshot(arguments);

    // Session-aware tools
    if (name == agent::kToolReadScreen)       return handleReadScreen(arguments);
    if (name == agent::kToolGetCursorPosition) return handleGetCursorPosition(arguments);
    if (name == agent::kToolGetFieldAt)       return handleGetFieldAt(arguments);
    if (name == agent::kToolSendKeys)         return handleSendKeys(arguments);
    if (name == agent::kToolRunScript)        return handleRunScript(arguments);
    if (name == agent::kToolLogin)            return handleLogin(arguments);

    // Session-independent
    if (name == agent::kToolListFiles)        return handleListFiles(arguments);
    if (name == agent::kToolReadFile)         return handleReadFile(arguments);
    if (name == agent::kToolWriteFile)        return handleWriteFile(arguments);

    // Deprecated / unavailable
    if (name == agent::kToolConnect)
        return makeResult("The 'connect' tool has been replaced by 'create_session'.", true);
    if (name == agent::kToolGenerateScript)
        return makeResult("generate_5250script is not available over MCP.", true);

    return makeResult("Unknown tool: " + name, true);
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
    auto *info = m_registry->session(sessionId);
    if (!info || !info->displayWidget) {
        MCP_ERROR(QString("Session not found: %1").arg(sessionId));
        errorResult = makeResult("Session not found or has been closed: " + sessionId, true);
        return nullptr;
    }
    return info->displayWidget;
}

// ---------------------------------------------------------------------------
// Session lifecycle tools
// ---------------------------------------------------------------------------

QJsonObject McpToolHandler::handleCreateSession(const QJsonObject &args) {
    QString hostname = args.value("hostname").toString();
    if (hostname.isEmpty())
        return makeResult("No hostname provided.", true);

    quint16 port = static_cast<quint16>(args.value("port").toInt(23));
    bool useTLS = args.value("useTLS").toBool(false);

    MCP_LOG(QString("Creating session to %1:%2 (TLS=%3)").arg(hostname).arg(port).arg(useTLS));

    // Generate session ID
    QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Register session (widget will be set later via onSessionCreated)
    McpSessionInfo info;
    info.sessionId = sessionId;
    info.hostname = hostname;
    info.port = port;
    info.useTLS = useTLS;
    m_registry->addSession(info);

    // Set up wait for session creation. Mark busy to reject reentrant
    // tool calls during the event loop.
    m_busy = true;
    m_pendingSessionId = sessionId;
    m_sessionCreated = false;

    QEventLoop loop;
    QTimer::singleShot(15000, &loop, [&]() { loop.quit(); });

    // Emit the request — MainWindow creates the tab synchronously on the
    // same thread, which calls onSessionCreated() before we reach loop.exec()
    emit createSessionRequested(sessionId, hostname, port, useTLS);

    if (!m_sessionCreated)
        loop.exec();

    m_pendingSessionId.clear();
    m_busy = false;

    if (!m_sessionCreated) {
        MCP_ERROR(QString("Session creation timed out for %1:%2").arg(hostname).arg(port));
        m_registry->removeSession(sessionId);
        return makeResult("Session creation timed out.", true);
    }

    MCP_LOG(QString("Session created: %1 -> %2:%3").arg(sessionId, hostname).arg(port));
    return makeResult(QString("session_id: %1\nConnected to %2:%3")
                          .arg(sessionId, hostname).arg(port));
}

QJsonObject McpToolHandler::handleCloseSession(const QJsonObject &args) {
    QString sessionId = args.value("session_id").toString();
    if (sessionId.isEmpty())
        return makeResult("Missing required parameter: session_id", true);

    if (!m_registry->hasSession(sessionId))
        return makeResult("Session not found: " + sessionId, true);

    MCP_LOG(QString("Closing session: %1").arg(sessionId));
    // Remove from registry first to prevent dangling widget pointers.
    // onCloseTabRequested also calls onSessionClosed→removeSession, but
    // the second remove is a harmless no-op.
    m_registry->removeSession(sessionId);
    emit closeSessionRequested(sessionId);

    return makeResult("Session closed: " + sessionId);
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

QString McpToolHandler::readScreenText(ui::widgets::Q5250ScreenWidget *widget) {
    if (!widget) return {};

    // MCP server runs on the main GUI thread, so we can access widgets directly.
    QString screenText;
    auto *buf = widget->screenBuffer();
    if (!buf) return {};
    for (int r = 0; r < buf->rows(); ++r) {
        QString line;
        for (int c = 0; c < buf->cols(); ++c) {
            uint8_t ch = buf->character(r, c);
            line += core::EBCDIC::ebcdicToChar(ch);
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
    QByteArray data = buf->getFieldData(field);
    QString fieldText;
    for (uint8_t byte : data)
        fieldText += core::EBCDIC::ebcdicToChar(byte);
    QString resultText = QString("startRow: %1, startCol: %2, length: %3, protected: %4, modified: %5, text: \"%6\"")
        .arg(field.startRow).arg(field.startCol).arg(field.length)
        .arg(field.protected_field ? "true" : "false")
        .arg(field.modified ? "true" : "false")
        .arg(fieldText.trimmed());
    return makeResult(resultText);
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

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return makeResult("Failed to read file: " + file.errorString(), true);

    QString content = QString::fromUtf8(file.readAll());
    file.close();
    return makeResult(content);
}

QJsonObject McpToolHandler::handleWriteFile(const QJsonObject &args) {
    QString path = args.value("path").toString();
    QString content = args.value("content").toString();
    if (path.isEmpty())
        return makeResult("No file path provided.", true);

    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return makeResult("Failed to write file: " + file.errorString(), true);

    file.write(content.toUtf8());
    file.close();
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

    // Run script with a local event loop. Mark busy to reject reentrant
    // tool calls that could delete the widget while the script runs.
    m_busy = true;

    bool success = false;
    QString log;
    bool done = false;

    // Use QPointer to detect widget destruction during execution.
    QPointer<ui::widgets::Q5250ScreenWidget> widgetGuard = widget;

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
    QTimer::singleShot(30000, &loop, [&]() {
        if (done) return;
        log = "Script execution timed out after 30 seconds.";
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

    m_busy = false;

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
    QStringList scriptLines;
    QRegularExpression tokenRe("\"([^\"]*)\"|\\S+");
    auto it = tokenRe.globalMatch(keys);
    while (it.hasNext()) {
        auto match = it.next();
        if (!match.captured(1).isNull()) {
            QString text = match.captured(1);
            text.replace('"', "\\\"");
            scriptLines << QString("TYPE \"%1\"").arg(text);
        } else
            scriptLines << QString("PRESS %1").arg(match.captured(0).toUpper());
    }

    // Build args with session_id preserved
    QJsonObject scriptArgs;
    scriptArgs["script"] = scriptLines.join('\n');
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

QJsonObject McpToolHandler::handleLogin(const QJsonObject &args) {
    // Validate session_id first
    QJsonObject err;
    auto *widget = resolveSession(args, err);
    if (!widget) return err;

    QString username = args.value("username").toString();
    QString password = args.value("password").toString();
    if (username.isEmpty())
        return makeResult("No username provided.", true);

    // Escape double quotes in credentials to prevent script injection
    QString safeUser = username;
    safeUser.replace('"', "\\\"");
    QString safePass = password;
    safePass.replace('"', "\\\"");

    QString script = QString(
        "GLOBAL EXPECT_TIMEOUT 10000\n"
        "EXPECT KEYBOARD UNLOCKED\n"
        "WAIT 500\n"
        "MOVE CURSOR AT INPUTFIELD 1\n"
        "TYPE \"%1\"\n"
        "PRESS TAB\n"
        "TYPE \"%2\"\n"
        "PRESS ENTER\n"
        "WAIT 1000\n"
        "EXPECT KEYBOARD UNLOCKED\n"
    ).arg(safeUser, safePass);

    QJsonObject scriptArgs;
    scriptArgs["script"] = script;
    scriptArgs["session_id"] = args.value("session_id");
    return handleRunScript(scriptArgs);
}

} // namespace mcp
