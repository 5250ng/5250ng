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
#include "core/ebcdic.h"
#include "session/config.h"
#include "ui/rendering/tn5250_command_handler.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

using session::PcCommandPolicy;
using ui::rendering::TN5250CommandHandler;
using ui::widgets::Q5250ScreenWidget;

// Regression tests for issue #251: a STRPCCMD alert or prompt is a modal
// dialog shown before ENTER goes back to the host. On an MCP-controlled
// session nobody dismisses it, so the host program and the MCP request that
// ran it hang.
class TestStrpccmdUnattended : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void cleanup();

    void testUnattendedDenyAndAlertShowsNoDialog();
    void testUnattendedPromptShowsNoDialogAndRunsNothing();
    void testAttendedReadOnlyScreenStillAlerts();
    void testUnattendedAllowAlwaysStillRuns();

  private:
    // Sends STRPCCMD `command` and returns whether a modal dialog appeared
    // (it is closed after 300 ms so the test never hangs)
    bool runCommand(const QString &command = QStringLiteral("echo hi"));

    Q5250ScreenWidget *m_widget = nullptr;
    TN5250CommandHandler *m_handler = nullptr;
    core::CommandRunner *m_runner = nullptr;
    int m_sent = 0;
};

void TestStrpccmdUnattended::init() {
    m_widget = new Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
    m_handler = new TN5250CommandHandler(this);
    m_handler->setDisplayWidget(m_widget);
    m_runner = new core::CommandRunner(this);
    m_handler->setCommandRunner(m_runner);
    m_sent = 0;
    m_handler->setSendToHostCallback([this](const QByteArray &) { ++m_sent; });
}

void TestStrpccmdUnattended::cleanup() {
    delete m_handler;
    delete m_runner;
    delete m_widget;
}

bool TestStrpccmdUnattended::runCommand(const QString &command) {
    bool dialogShown = false;
    QTimer::singleShot(300, this, [&]() {
        if (QWidget *modal = QApplication::activeModalWidget()) {
            dialogShown = true;
            modal->close();
        }
    });
    QByteArray ebcdic;
    for (QChar c : command)
        ebcdic.append(static_cast<char>(core::EBCDIC::charToEBCDIC(c)));
    m_handler->onStrpccmdRequested(true, ebcdic);
    QTest::qWait(400);
    return dialogShown;
}

void TestStrpccmdUnattended::testUnattendedDenyAndAlertShowsNoDialog() {
    m_handler->setUnattended(true);
    m_handler->setPcCommandPolicy(PcCommandPolicy::DenyAndAlert);
    QVERIFY(!runCommand());
    QCOMPARE(m_sent, 1);
}

void TestStrpccmdUnattended::testUnattendedPromptShowsNoDialogAndRunsNothing() {
#ifdef Q_OS_WIN
    QSKIP("uses /usr/bin/touch");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString marker = dir.filePath(QStringLiteral("ran"));
    m_handler->setUnattended(true);
    m_handler->setPcCommandPolicy(PcCommandPolicy::AllowWithPrompt);
    QVERIFY(!runCommand(QStringLiteral("/usr/bin/touch ") + marker));
    QTest::qWait(300);
    QVERIFY(!QFile::exists(marker));
    QCOMPARE(m_sent, 1);
#endif
}

void TestStrpccmdUnattended::testAttendedReadOnlyScreenStillAlerts() {
    // Read-only is a user setting on an interactive session: the user is
    // there, so the alert is still shown
    m_widget->setReadOnly(true);
    m_handler->setPcCommandPolicy(PcCommandPolicy::DenyAndAlert);
    QVERIFY(runCommand());
    QCOMPARE(m_sent, 1);
}

void TestStrpccmdUnattended::testUnattendedAllowAlwaysStillRuns() {
    // AllowAlways needs no answer: the user's choice still applies
#ifdef Q_OS_WIN
    QSKIP("uses /usr/bin/touch");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString marker = dir.filePath(QStringLiteral("ran"));
    m_handler->setUnattended(true);
    m_handler->setPcCommandPolicy(PcCommandPolicy::AllowAlways);
    QVERIFY(!runCommand(QStringLiteral("/usr/bin/touch ") + marker));
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 3000);
    QCOMPARE(m_sent, 1);
#endif
}

QTEST_MAIN(TestStrpccmdUnattended)
#include "test_strpccmd_unattended.moc"
