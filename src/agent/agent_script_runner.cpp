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

#include "agent_script_runner.h"
#include <5250script/script_executor.h>
#include <5250script/script_parser.h>
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer_adapter.h"
#include <QApplication>
#include <QKeyEvent>
#include <QPointer>

namespace agent {

AgentScriptRunner::AgentScriptRunner(ui::widgets::Q5250ScreenWidget *display,
                                     QObject *parent)
    : QObject(parent), m_display(display) {}

AgentScriptRunner::~AgentScriptRunner() {
    stop();
}

void AgentScriptRunner::runScript(const QString &scriptText) {
    if (m_running) {
        emit finished(false, "A script is already running.");
        return;
    }

    // If the target widget was destroyed between construction and this call
    // (e.g. the tab was closed), abort cleanly rather than dereferencing a
    // dead pointer when we wire up signals below.
    if (!m_display || !m_display->screenBuffer()) {
        emit finished(false, "Terminal session is unavailable.");
        return;
    }

    // Parse
    core::scripting::ScriptParser parser;
    auto parseResult = parser.parse(scriptText);
    if (parseResult.hasErrors()) {
        QStringList errMsgs;
        for (const auto &e : parseResult.errors)
            errMsgs << QStringLiteral("Line %1: %2").arg(e.line).arg(e.message);
        emit finished(false, "Parse error:\n" + errMsgs.join("\n"));
        return;
    }

    m_logMessages.clear();
    m_running = true;

    // Create executor and screen adapter
    m_executor = new core::scripting::ScriptExecutor(this);
    m_screenAdapter = new core::scripting::ScreenBufferAdapter(m_display);
    m_executor->setScreen(m_screenAdapter);

    QPointer<ui::widgets::Q5250ScreenWidget> displayGuard = m_display;
    QPointer<core::scripting::ScriptExecutor> execGuard = m_executor;

    // Key injection (bypasses read-only via mcpInjecting flag)
    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::injectKeyPress, this,
            [displayGuard](int key, Qt::KeyboardModifiers mods, const QString &text) {
                if (displayGuard.isNull()) return;
                displayGuard->setMcpInjecting(true);
                QKeyEvent ev(QEvent::KeyPress, key, mods, text);
                QApplication::sendEvent(displayGuard.data(), &ev);
                displayGuard->setMcpInjecting(false);
            }));

    // AID key injection (bypasses read-only via mcpInjecting flag)
    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::injectAIDKey, this,
            [displayGuard](uint8_t aid) {
                if (displayGuard.isNull()) return;
                int qtKey = 0;
                Qt::KeyboardModifiers mods = Qt::NoModifier;
                if (aid == 0xF1) { qtKey = Qt::Key_Return; }
                else if (aid == 0x70) { qtKey = Qt::Key_Escape; mods = Qt::ControlModifier; }
                else if (aid == 0x71) { qtKey = Qt::Key_SysReq; }
                else if (aid >= 0x31 && aid <= 0x3C) { qtKey = Qt::Key_F1 + (aid - 0x31); }
                else if (aid >= 0xB1 && aid <= 0xBC) { qtKey = Qt::Key_F1 + (aid - 0xB1); mods = Qt::ShiftModifier; }
                else if (aid == 0xF3) { qtKey = Qt::Key_F1; mods = Qt::ControlModifier; }
                // Must be the inverse of KeyboardEncoder, which maps PC PageUp
                // to Roll Down (0xF4) and PageDown to Roll Up (0xF5).
                else if (aid == 0xF4) { qtKey = Qt::Key_PageUp; }
                else if (aid == 0xF5) { qtKey = Qt::Key_PageDown; }
                else if (aid == 0xF6) { qtKey = Qt::Key_Print; }
                else if (aid == 0xBD) { qtKey = Qt::Key_Pause; mods = Qt::ControlModifier; }
                else return;
                displayGuard->setMcpInjecting(true);
                QKeyEvent ev(QEvent::KeyPress, qtKey, mods, QString());
                QApplication::sendEvent(displayGuard.data(), &ev);
                displayGuard->setMcpInjecting(false);
            }));

    // Cursor movement
    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::moveCursor, this,
            [displayGuard](int row, int col) {
                if (displayGuard.isNull()) return;
                displayGuard->moveCursor(row, col);
            }));

    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::moveCursorStep, this,
            [displayGuard](const QString &dir) {
                if (displayGuard.isNull()) return;
                if (dir == "UP") displayGuard->moveCursorUp();
                else if (dir == "DOWN") displayGuard->moveCursorDown();
                else if (dir == "LEFT") displayGuard->moveCursorLeft();
                else if (dir == "RIGHT") displayGuard->moveCursorRight();
            }));

    // GOTO INPUTFIELD
    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::gotoInputField, this,
            [displayGuard](int fieldIndex) {
                if (displayGuard.isNull()) return;
                auto *buf = displayGuard->screenBuffer();
                if (!buf) return;
                const auto &fields = buf->fields();
                QVector<const ui::widgets::ScreenBuffer::Field *> inputFields;
                for (const auto &f : fields) {
                    if (!f.protected_field)
                        inputFields.append(&f);
                }
                if (inputFields.isEmpty()) return;
                if (fieldIndex == -1) {
                    displayGuard->moveToNextField();
                } else if (fieldIndex == -2) {
                    displayGuard->moveToPreviousField();
                } else if (fieldIndex >= 1 && fieldIndex <= inputFields.size()) {
                    auto *f = inputFields[fieldIndex - 1];
                    displayGuard->moveCursor(f->startRow, f->startCol);
                }
            }));

    // Screen change notifications
    m_connections.append(
        connect(m_display->screenBuffer(), &ui::widgets::ScreenBuffer::screenChanged,
            m_executor, &core::scripting::ScriptExecutor::notifyScreenChanged));

    m_connections.append(
        connect(m_display, &ui::widgets::Q5250ScreenWidget::terminalStateChanged,
            m_executor, &core::scripting::ScriptExecutor::notifyTerminalStateChanged));

    // Log messages
    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::logMessage, this,
            [this](const QString &msg) {
                m_logMessages.append(msg);
            }));

    // Execution finished
    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::executionFinished, this,
            [this]() {
                QString log = m_logMessages.join("\n");
                cleanup();
                emit finished(true, log.isEmpty() ? "Script completed successfully." : log);
            }));

    // Execution error
    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::executionError, this,
            [this](int line, const QString &msg) {
                QString log = m_logMessages.join("\n");
                if (!log.isEmpty()) log += "\n";
                log += QStringLiteral("Error at line %1: %2").arg(line).arg(msg);
                cleanup();
                emit finished(false, log);
            }));

    // PAUSE and INPUT are not supported in agent scripts — stop with error
    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::pauseRequested, this,
            [this, execGuard]() {
                if (!execGuard.isNull()) execGuard->stop();
                QString log = m_logMessages.join("\n");
                if (!log.isEmpty()) log += "\n";
                log += "Error: PAUSE is not supported in agent-generated scripts.";
                cleanup();
                emit finished(false, log);
            }));

    m_connections.append(
        connect(m_executor, &core::scripting::ScriptExecutor::inputRequested, this,
            [this, execGuard](const QStringList &, const QStringList &) {
                if (!execGuard.isNull()) execGuard->stop();
                QString log = m_logMessages.join("\n");
                if (!log.isEmpty()) log += "\n";
                log += "Error: INPUT is not supported in agent-generated scripts.";
                cleanup();
                emit finished(false, log);
            }));

    m_executor->execute(parseResult);
}

void AgentScriptRunner::stop() {
    if (m_executor && m_running) {
        m_executor->stop();
    }
    if (m_running) {
        QString log = m_logMessages.join("\n");
        if (!log.isEmpty()) log += "\n";
        log += "Script stopped.";
        cleanup();
        emit finished(false, log);
    }
}

bool AgentScriptRunner::isRunning() const {
    return m_running;
}

void AgentScriptRunner::cleanup() {
    m_running = false;

    // Disconnect all signals first — prevents any further callbacks
    // into the executor or screen adapter after this point.
    for (auto &conn : m_connections)
        QObject::disconnect(conn);
    m_connections.clear();

    // Defer actual deletion to after the current signal chain unwinds.
    // cleanup() is called from signal handlers (executionFinished, executionError)
    // which may still be on the call stack. Deleting the executor or screen
    // adapter here can cause use-after-free.
    auto *adapter = m_screenAdapter;
    m_screenAdapter = nullptr;
    auto *executor = m_executor;
    m_executor = nullptr;

    // Reparent the executor so deleting the runner doesn't double-free it.
    if (executor)
        executor->setParent(nullptr);

    // Defer deletion to after the current signal chain unwinds.
    QTimer::singleShot(0, qApp, [adapter, executor]() {
        delete adapter;
        delete executor;
    });
}

} // namespace agent
