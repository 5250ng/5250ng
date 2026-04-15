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

#include <QObject>
#include <QPointer>
#include <QStringList>

namespace core::scripting {
class ScriptExecutor;
class ScreenBufferAdapter;
} // namespace core::scripting

namespace ui::widgets {
class Q5250ScreenWidget;
}

namespace agent {

/// Runs a 5250script on a terminal session and reports the result.
/// Encapsulates the ScriptParser + ScriptExecutor + signal wiring.
class AgentScriptRunner : public QObject {
    Q_OBJECT

  public:
    explicit AgentScriptRunner(ui::widgets::Q5250ScreenWidget *display,
                               QObject *parent = nullptr);
    ~AgentScriptRunner();

    void runScript(const QString &scriptText);
    void stop();
    bool isRunning() const;

  signals:
    /// Emitted when script execution completes.
    /// @param success true if script finished without errors
    /// @param log combined log messages and error info
    void finished(bool success, const QString &log);

  private:
    void cleanup();

    // QPointer: the widget may be destroyed by the tab closing while a
    // script has been queued but has not yet started (or between two runs).
    QPointer<ui::widgets::Q5250ScreenWidget> m_display;
    core::scripting::ScriptExecutor *m_executor = nullptr;
    core::scripting::ScreenBufferAdapter *m_screenAdapter = nullptr;
    QStringList m_logMessages;
    bool m_running = false;

    // Signal connections (to disconnect on cleanup)
    QVector<QMetaObject::Connection> m_connections;
};

} // namespace agent
