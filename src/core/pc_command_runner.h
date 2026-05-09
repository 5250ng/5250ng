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
#include <QString>
#include <functional>

class QWidget;

namespace core {

// Runs a host-issued STRPCCMD command on the local PC, with a policy gate.
// Two modes:
//   - Wait: starts the process, blocks up to a timeout, returns the exit code.
//   - NoWait: launches the process detached and returns immediately.
// In both modes the runner refuses to do anything when the feature is
// disabled, and (when enabled) consults a confirm callback to ask the user
// per command. The callback is injected so unit tests can supply an
// auto-accept/auto-deny stub without touching real Qt dialogs.
class PcCommandRunner : public QObject {
    Q_OBJECT
  public:
    enum class Mode {
        Wait,
        NoWait,
    };

    enum class Outcome {
        DisabledByPolicy,  // pcCommandEnabled=false; nothing was attempted
        DeniedByUser,      // user clicked Deny in the confirm dialog
        StartFailed,       // QProcess refused to launch (bad binary, etc.)
        TimedOut,          // wait mode hit the timeout before exit
        Completed,         // process exited within the timeout (Wait mode)
        Launched,          // process was started detached (NoWait mode)
    };

    struct Result {
        Outcome outcome;
        int exitCode = 0;  // Only meaningful for Outcome::Completed
    };

    // Confirm callback returns true to allow, false to deny. Receives the
    // host name (for "this command was requested by host X") and the full
    // command string. The callback is responsible for showing UI; the runner
    // does not pop dialogs itself.
    using ConfirmFn = std::function<bool(const QString &hostname, const QString &command)>;

    explicit PcCommandRunner(QObject *parent = nullptr);

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void setHostname(const QString &hostname) { m_hostname = hostname; }
    void setConfirmCallback(ConfirmFn fn) { m_confirm = std::move(fn); }

    // Wait-mode timeout. Defaults to 60 seconds. Set to -1 to disable
    // (waits forever — use only in tests).
    void setWaitTimeoutMs(int ms) { m_waitTimeoutMs = ms; }

    // Runs the command. Returns the outcome. Wait mode blocks the calling
    // thread; the caller is expected to be on the UI thread for the dialog
    // to render but should be aware of the blocking nature.
    Result run(const QString &command, Mode mode);

  private:
    bool m_enabled = false;
    QString m_hostname;
    ConfirmFn m_confirm;
    int m_waitTimeoutMs = 60000;
};

} // namespace core
