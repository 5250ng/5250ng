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

#include <QFile>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using core::CommandRunner;

class TestCommandRunner : public QObject {
    Q_OBJECT

  private slots:
    void testDisabledByDefault();
    void testDisabledRefusesEvenIfConfirmAccepts();
    void testDeniedByUserDoesNotRun();
    void testEnabledAndAllowedRunsCommand();
    void testNoWaitLaunchesDetachedAndReturnsImmediately();
    void testEmptyCommandIsRejected();
};

void TestCommandRunner::testDisabledByDefault() {
    CommandRunner runner;
    QVERIFY(!runner.isEnabled());

    bool confirmCalled = false;
    runner.setConfirmCallback([&](const QString &, const QString &) {
        confirmCalled = true;
        return true;
    });

    auto result = runner.run("/bin/echo hello", CommandRunner::Mode::Wait);
    QCOMPARE(result.outcome, CommandRunner::Outcome::DisabledByPolicy);
    QVERIFY(!confirmCalled);
}

void TestCommandRunner::testDisabledRefusesEvenIfConfirmAccepts() {
    // Defence in depth: even if some upstream code wires a permissive confirm
    // callback, the policy gate must still refuse when the runner is disabled.
    CommandRunner runner;
    runner.setConfirmCallback([](const QString &, const QString &) { return true; });

    auto result = runner.run("/bin/echo hello", CommandRunner::Mode::NoWait);
    QCOMPARE(result.outcome, CommandRunner::Outcome::DisabledByPolicy);
}

void TestCommandRunner::testDeniedByUserDoesNotRun() {
    CommandRunner runner;
    runner.setEnabled(true);
    bool confirmCalled = false;
    runner.setConfirmCallback([&](const QString &, const QString &) {
        confirmCalled = true;
        return false;
    });

    auto result = runner.run("/bin/echo hello", CommandRunner::Mode::Wait);
    QCOMPARE(result.outcome, CommandRunner::Outcome::DeniedByUser);
    QVERIFY(confirmCalled);
}

void TestCommandRunner::testEnabledAndAllowedRunsCommand() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString marker = tmp.filePath("ran.txt");

    CommandRunner runner;
    runner.setEnabled(true);
    runner.setConfirmCallback([](const QString &, const QString &) { return true; });

#ifdef Q_OS_WIN
    const QString cmd = QString("cmd /c echo > \"%1\"").arg(marker);
#else
    const QString cmd = QString("/usr/bin/touch \"%1\"").arg(marker);
#endif

    auto result = runner.run(cmd, CommandRunner::Mode::Wait);
    QCOMPARE(result.outcome, CommandRunner::Outcome::Completed);
    QVERIFY2(QFile::exists(marker), "command should have created the marker file");
}

void TestCommandRunner::testNoWaitLaunchesDetachedAndReturnsImmediately() {
    CommandRunner runner;
    runner.setEnabled(true);
    runner.setConfirmCallback([](const QString &, const QString &) { return true; });

    // Use a command that lives long enough for QProcess::startDetached to
    // confirm the launch on every supported platform. macOS Qt 6.11 uses
    // posix_spawn followed by a non-blocking waitpid; sub-millisecond
    // commands like /bin/true can exit before that check and produce a
    // false StartFailed result. A short sleep is reliable everywhere.
#ifdef Q_OS_WIN
    const QString cmd = "ping -n 2 127.0.0.1";
#else
    const QString cmd = "/bin/sleep 1";
#endif

    auto result = runner.run(cmd, CommandRunner::Mode::NoWait);
    QCOMPARE(result.outcome, CommandRunner::Outcome::Launched);
}

void TestCommandRunner::testEmptyCommandIsRejected() {
    CommandRunner runner;
    runner.setEnabled(true);
    runner.setConfirmCallback([](const QString &, const QString &) { return true; });

    auto result = runner.run("   ", CommandRunner::Mode::Wait);
    QCOMPARE(result.outcome, CommandRunner::Outcome::StartFailed);
}

QTEST_MAIN(TestCommandRunner)
#include "test_command_runner.moc"
