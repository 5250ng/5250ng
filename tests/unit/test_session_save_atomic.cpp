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

#include "session/config.h"
#include "session/manager.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>
#include <QtTest/QtTest>
#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/resource.h>
#endif

using session::SessionConfig;
using session::SessionManager;

// A save that fails part-way must leave the previously saved profile on
// disk unchanged and report the failure.
class TestSessionSaveAtomic : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void failedWriteKeepsPreviousProfile() {
#ifndef Q_OS_UNIX
        QSKIP("Needs RLIMIT_FSIZE to make the write fail");
#else
        const QString name = QStringLiteral("atomic-save-")
                             + QUuid::createUuid().toString(QUuid::WithoutBraces);
        SessionConfig config;
        config.setName(name);
        config.setHostname(QStringLiteral("before.example.com"));

        SessionManager manager;
        QVERIFY(manager.saveSession(config));
        const QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       + QStringLiteral("/sessions"));
        const QStringList files = dir.entryList({name + "-*.json"}, QDir::Files);
        QCOMPARE(files.size(), 1);
        const QString path = dir.filePath(files.constFirst());
        QFile before(path);
        QVERIFY(before.open(QIODevice::ReadOnly));
        const QByteArray original = before.readAll();
        before.close();
        QVERIFY(original.size() > 64);

        // Make every write past 64 bytes fail (EFBIG instead of SIGXFSZ),
        // as a full disk would.
        struct rlimit saved;
        QCOMPARE(getrlimit(RLIMIT_FSIZE, &saved), 0);
        auto oldHandler = std::signal(SIGXFSZ, SIG_IGN);
        struct rlimit small = saved;
        small.rlim_cur = 64;
        QCOMPARE(setrlimit(RLIMIT_FSIZE, &small), 0);

        config.setHostname(QStringLiteral("after.example.com"));
        config.setSessionVariables({{QStringLiteral("PADDING"), QString(512, 'x')}});
        const bool saved_ok = manager.saveSession(config);

        setrlimit(RLIMIT_FSIZE, &saved);
        std::signal(SIGXFSZ, oldHandler);

        QFile after(path);
        QVERIFY(after.open(QIODevice::ReadOnly));
        const QByteArray onDisk = after.readAll();
        after.close();
        QVERIFY(manager.deleteSession(name));

        QVERIFY2(!saved_ok, "saveSession reported success for a failed write");
        QCOMPARE(onDisk, original);
        // No temporary file is left behind.
        QCOMPARE(dir.entryList({name + "-*"}, QDir::Files | QDir::Hidden).size(), 0);
#endif
    }
};

QTEST_GUILESS_MAIN(TestSessionSaveAtomic)
#include "test_session_save_atomic.moc"
