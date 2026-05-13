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

/// Runs a host-issued `STRPCCMD` command on the local workstation behind
/// a policy gate and an optional user-confirmation callback.
///
/// `STRPCCMD` (Start PC Command) is the AS/400 / IBM i command that asks
/// the connected emulator to execute a program on the operator's machine.
/// The "PC" in the host command name is historical — the operator
/// workstation today is just as likely to run macOS or Linux as Windows,
/// and `CommandRunner` is the cross-platform local-side executor.
///
/// Behaviour overview:
/// - When disabled (`setEnabled(false)`, the default), every `run()` call
///   returns `Outcome::DisabledByPolicy` without invoking the confirm
///   callback or touching `QProcess`. This is the safe default; the
///   emulator's settings UI is the only thing that should flip it.
/// - When enabled, each command goes through `m_confirm` (if set). The
///   callback may show a dialog and must return `true` to allow the
///   command, `false` to deny it. The runner itself never pops UI — the
///   callback is the seam where UI is injected, which keeps the runner
///   easy to unit-test.
/// - On approval, `run()` tokenises the command into an argv vector and
///   hands it to `QProcess` without involving a shell. This intentionally
///   neuters shell metacharacters (`;`, `|`, backticks, `$()`) embedded
///   in the host-provided payload.
///
/// The class is intended to be owned by a `Session` and have its
/// hostname kept in sync with the active TN5250 connection so the
/// confirm callback can surface which host requested the command.
class CommandRunner : public QObject {
    Q_OBJECT
  public:
    /// How `run()` interacts with the launched process.
    enum class Mode {
        /// Block the calling thread up to the configured wait timeout and
        /// return the process exit code. Suitable when the host issued
        /// `STRPCCMD ... PAUSE(*YES)` and is waiting on the result.
        Wait,
        /// `QProcess::startDetached` and return immediately. Suitable
        /// when the host issued `STRPCCMD ... PAUSE(*NO)`.
        NoWait,
    };

    /// Terminal state of a `run()` invocation.
    enum class Outcome {
        /// Feature disabled in settings; nothing was attempted. The
        /// confirm callback is NOT consulted on this path.
        DisabledByPolicy,
        /// User clicked Deny in the confirm dialog (or callback returned
        /// false for any other reason). No process was launched.
        DeniedByUser,
        /// `QProcess::start` / `startDetached` refused to launch — most
        /// commonly because the program path does not exist, is not
        /// executable, or the tokeniser rejected the input (e.g. an
        /// unterminated quoted argument).
        StartFailed,
        /// Wait mode hit `m_waitTimeoutMs` before the child exited; the
        /// child was killed and reaped.
        TimedOut,
        /// Wait mode: child exited within the timeout. `Result::exitCode`
        /// carries the process exit code.
        Completed,
        /// NoWait mode: child was successfully started detached. The
        /// runner does not track the child after this point.
        Launched,
    };

    /// Outcome plus, when applicable, the child process exit code.
    struct Result {
        Outcome outcome;
        /// Only meaningful for `Outcome::Completed`. Zero for every
        /// other outcome.
        int exitCode = 0;
    };

    /// Synchronous confirmation hook. Receives the currently-connected
    /// host and the full unparsed command string. Returns `true` to
    /// allow the command, `false` to deny.
    ///
    /// The runner does NOT pop dialogs itself — the callback is the
    /// single point where UI is wired in. This keeps the runner free of
    /// Qt widget dependencies and allows unit tests to substitute an
    /// auto-accept / auto-deny stub.
    using ConfirmFn = std::function<bool(const QString &hostname,
                                          const QString &command)>;

    /// Construct a disabled runner with no confirm callback set.
    explicit CommandRunner(QObject *parent = nullptr);

    /// Toggle the policy gate. Must be `true` for any command to run.
    void setEnabled(bool enabled) { m_enabled = enabled; }

    /// Current state of the policy gate.
    bool isEnabled() const { return m_enabled; }

    /// Set the host name passed to the confirm callback. Typically
    /// updated whenever the owning session connects to a new host.
    void setHostname(const QString &hostname) { m_hostname = hostname; }

    /// Install the user-confirmation callback. Pass an empty
    /// `std::function` (or do not call this method) to treat every
    /// enabled command as approved — useful for tests, never for
    /// production builds.
    void setConfirmCallback(ConfirmFn fn) { m_confirm = std::move(fn); }

    /// Wait-mode timeout in milliseconds. Defaults to 60000 (60s).
    /// Set to a negative value to disable the timeout entirely; use
    /// only in tests, since a runaway host command would otherwise
    /// block the UI thread forever.
    void setWaitTimeoutMs(int ms) { m_waitTimeoutMs = ms; }

    /// Tokenise `command`, consult the policy gate and confirm
    /// callback, and execute via `QProcess`. In Wait mode the calling
    /// thread blocks until the child exits or the timeout fires; in
    /// NoWait mode the call returns as soon as the launch succeeds or
    /// fails.
    ///
    /// The caller is normally on the UI thread (because the confirm
    /// callback typically shows a modal dialog) — be aware that Wait
    /// mode will freeze the UI for up to the configured timeout.
    Result run(const QString &command, Mode mode);

  private:
    bool m_enabled = false;
    QString m_hostname;
    ConfirmFn m_confirm;
    int m_waitTimeoutMs = 60000;
};

} // namespace core
