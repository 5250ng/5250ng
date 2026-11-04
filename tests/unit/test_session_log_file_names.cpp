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

#include "core/session_logger.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using core::SessionLogger;

// Regression tests for issue #395: session log names kept every host-name
// character but '.', so IPv6 hosts gave names invalid on Windows, and two
// logs started in the same second shared one file.
class TestSessionLogFileNames : public QObject {
    Q_OBJECT

  private slots:
    void testIpv6HostGivesValidName_data() {
        QTest::addColumn<QString>("host");
        QTest::newRow("ipv6") << "fe80::1%eth0";
        QTest::newRow("bracketed") << "[::1]";
        QTest::newRow("ipv4") << "192.168.1.109";
        QTest::newRow("name") << "ibmi.example.com";
        QTest::newRow("empty") << "";
    }

    void testIpv6HostGivesValidName() {
        QFETCH(QString, host);
        QTemporaryDir dir;
        const QString path = SessionLogger::logFilePath(dir.path(), host, QDateTime::currentDateTime());
        const QString name = QFileInfo(path).fileName();
        // Only characters valid in a file name on Windows, macOS and Linux
        QVERIFY2(QRegularExpression(QStringLiteral("^[A-Za-z0-9_.-]+$")).match(name).hasMatch(),
                 qPrintable(name));
        QVERIFY(name.endsWith(QStringLiteral(".log")));
        QCOMPARE(QFileInfo(path).absolutePath(), QFileInfo(dir.path()).absoluteFilePath());
    }

    void testNameFormat() {
        QTemporaryDir dir;
        const QDateTime when(QDate(2026, 11, 3), QTime(16, 38, 0, 123));
        const QString name =
            QFileInfo(SessionLogger::logFilePath(dir.path(), QStringLiteral("192.168.1.109"), when)).fileName();
        QCOMPARE(name, QStringLiteral("session_192_168_1_109_20261103_163800_123.log"));
        // A very long host is shortened so the name stays within file-system limits
        const QString longName =
            QFileInfo(SessionLogger::logFilePath(dir.path(), QString(300, 'a'), when)).fileName();
        QVERIFY(longName.size() < 120);
    }

    void testSameTimeGetsItsOwnFile() {
        QTemporaryDir dir;
        const QDateTime when = QDateTime::currentDateTime();
        const QString first = SessionLogger::logFilePath(dir.path(), QStringLiteral("host"), when);
        SessionLogger a;
        QVERIFY(a.start(first));
        const QString second = SessionLogger::logFilePath(dir.path(), QStringLiteral("host"), when);
        QVERIFY(second != first);
        QVERIFY(!QFileInfo::exists(second));
        a.stop();
    }
};

QTEST_MAIN(TestSessionLogFileNames)
#include "test_session_log_file_names.moc"
