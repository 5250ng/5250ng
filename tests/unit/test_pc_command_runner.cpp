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

#include "core/pc_command_runner.h"

#include <QFile>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using core::PcCommandRunner;

class TestPcCommandRunner : public QObject {
    Q_OBJECT

  private slots:
    void testDisabledByDefault();
    void testDisabledRefusesEvenIfConfirmAccepts();
    void testDeniedByUserDoesNotRun();
    void testEnabledAndAllowedRunsCommand();
    void testNoWaitLaunchesDetachedAndReturnsImmediately();
    void testEmptyCommandIsRejected();
};

void TestPcCommandRunner::testDisabledByDefault() {
    PcCommandRunner runner;
    QVERIFY(!runner.isEnabled());

    bool confirmCalled = false;
    runner.setConfirmCallback([&](const QString &, const QString &) {
        confirmCalled = true;
        return true;
    });

    auto result = runner.run("/bin/echo hello", PcCommandRunner::Mode::Wait);
    QCOMPARE(result.outcome, PcCommandRunner::Outcome::DisabledByPolicy);
    QVERIFY(!confirmCalled);
}

void TestPcCommandRunner::testDisabledRefusesEvenIfConfirmAccepts() {
    // Defence in depth: even if some upstream code wires a permissive confirm
    // callback, the policy gate must still refuse when pcCommandEnabled=false.
    PcCommandRunner runner;
    runner.setConfirmCallback([](const QString &, const QString &) { return true; });

    auto result = runner.run("/bin/echo hello", PcCommandRunner::Mode::NoWait);
    QCOMPARE(result.outcome, PcCommandRunner::Outcome::DisabledByPolicy);
}

void TestPcCommandRunner::testDeniedByUserDoesNotRun() {
    PcCommandRunner runner;
    runner.setEnabled(true);
    bool confirmCalled = false;
    runner.setConfirmCallback([&](const QString &, const QString &) {
        confirmCalled = true;
        return false;
    });

    auto result = runner.run("/bin/echo hello", PcCommandRunner::Mode::Wait);
    QCOMPARE(result.outcome, PcCommandRunner::Outcome::DeniedByUser);
    QVERIFY(confirmCalled);
}

void TestPcCommandRunner::testEnabledAndAllowedRunsCommand() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString marker = tmp.filePath("ran.txt");

    PcCommandRunner runner;
    runner.setEnabled(true);
    runner.setConfirmCallback([](const QString &, const QString &) { return true; });

#ifdef Q_OS_WIN
    const QString cmd = QString("cmd /c echo > \"%1\"").arg(marker);
#else
    const QString cmd = QString("/usr/bin/touch \"%1\"").arg(marker);
#endif

    auto result = runner.run(cmd, PcCommandRunner::Mode::Wait);
    QCOMPARE(result.outcome, PcCommandRunner::Outcome::Completed);
    QVERIFY2(QFile::exists(marker), "command should have created the marker file");
}

void TestPcCommandRunner::testNoWaitLaunchesDetachedAndReturnsImmediately() {
    PcCommandRunner runner;
    runner.setEnabled(true);
    runner.setConfirmCallback([](const QString &, const QString &) { return true; });

#ifdef Q_OS_WIN
    const QString cmd = "cmd /c exit 0";
#else
    const QString cmd = "/bin/true";
#endif

    auto result = runner.run(cmd, PcCommandRunner::Mode::NoWait);
    QCOMPARE(result.outcome, PcCommandRunner::Outcome::Launched);
}

void TestPcCommandRunner::testEmptyCommandIsRejected() {
    PcCommandRunner runner;
    runner.setEnabled(true);
    runner.setConfirmCallback([](const QString &, const QString &) { return true; });

    auto result = runner.run("   ", PcCommandRunner::Mode::Wait);
    QCOMPARE(result.outcome, PcCommandRunner::Outcome::StartFailed);
}

QTEST_MAIN(TestPcCommandRunner)
#include "test_pc_command_runner.moc"
