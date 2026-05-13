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

#include "core/command_runner.h"
#include "logger/logger.h"

#include <QProcess>
#include <QStringList>

namespace core {

CommandRunner::CommandRunner(QObject *parent) : QObject(parent) {}

/// Splits a `STRPCCMD` command string into program + arguments using
/// minimal shell-like quoting rules: bare tokens separated by
/// whitespace, with double-quoted spans treated as one token.
///
/// This intentionally avoids handing the string to a real shell —
/// `STRPCCMD` payloads come from the host and may contain shell
/// metacharacters (`;`, `|`, `$()`, backticks). `QProcess::start`'s
/// `(program, arguments)` overload executes the program directly and
/// does NOT re-interpret the arguments through a shell, which neuters
/// those metacharacters.
static QStringList splitCommand(const QString &command) {
    QStringList tokens;
    QString current;
    bool inQuotes = false;
    for (QChar c : command) {
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && c.isSpace()) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
            continue;
        }
        current.append(c);
    }
    if (!current.isEmpty()) tokens.append(current);
    return tokens;
}

CommandRunner::Result CommandRunner::run(const QString &command, Mode mode) {
    if (!m_enabled) {
        logger::Logger::instance()->warning(
            QString("CommandRunner: refused (feature disabled): %1").arg(command));
        return {Outcome::DisabledByPolicy, 0};
    }
    if (m_confirm && !m_confirm(m_hostname, command)) {
        logger::Logger::instance()->info(
            QString("CommandRunner: user denied command: %1").arg(command));
        return {Outcome::DeniedByUser, 0};
    }

    QStringList tokens = splitCommand(command);
    if (tokens.isEmpty()) {
        logger::Logger::instance()->warning(
            "CommandRunner: empty command after tokenisation, refusing to launch");
        return {Outcome::StartFailed, 0};
    }
    const QString program = tokens.takeFirst();
    const QStringList args = tokens;

    if (mode == Mode::NoWait) {
        qint64 pid = 0;
        const bool ok = QProcess::startDetached(program, args, QString(), &pid);
        if (!ok) {
            logger::Logger::instance()->warning(
                QString("CommandRunner: startDetached failed: %1").arg(command));
            return {Outcome::StartFailed, 0};
        }
        logger::Logger::instance()->info(
            QString("CommandRunner: launched detached pid=%1: %2")
                .arg(pid).arg(command));
        return {Outcome::Launched, 0};
    }

    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(5000)) {
        logger::Logger::instance()->warning(
            QString("CommandRunner: waitForStarted failed: %1").arg(command));
        return {Outcome::StartFailed, 0};
    }
    if (!proc.waitForFinished(m_waitTimeoutMs)) {
        proc.kill();
        proc.waitForFinished(1000);
        logger::Logger::instance()->warning(
            QString("CommandRunner: timed out after %1 ms: %2")
                .arg(m_waitTimeoutMs).arg(command));
        return {Outcome::TimedOut, 0};
    }
    const int code = proc.exitCode();
    logger::Logger::instance()->info(
        QString("CommandRunner: completed exit=%1: %2").arg(code).arg(command));
    return {Outcome::Completed, code};
}

} // namespace core
