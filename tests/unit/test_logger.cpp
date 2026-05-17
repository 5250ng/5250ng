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

#include "logger/logger.h"

#include <QtTest/QtTest>
#include <atomic>
#include <thread>
#include <vector>

using logger::Logger;
using logger::LogLevel;

class TestLogger : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        // Suppress console output during the storm; otherwise the suite
        // floods stderr with thousands of messages.
        Logger::instance()->enableConsoleOutput(false);
    }

    // Smoke test for the std::atomic<LogLevel> conversion of m_logLevel.
    // Hammer setLogLevel() and log() from many threads concurrently; the
    // logger must not crash, must not produce inconsistent state, and the
    // final logLevel() must be one of the values actually stored.
    //
    // A pure data-race (read torn against write) is not deterministically
    // observable in a normal unit test — it requires ThreadSanitizer.
    // This test instead exercises the concurrent code path so any future
    // regression that breaks the atomic contract (e.g. dropping back to a
    // plain enum) is at least likely to manifest as a stale read or, on a
    // weakly-ordered platform, a crash under TSan/ASan instrumentation.
    void testConcurrentSetLevelAndLogDoesNotCrash() {
        auto *log = Logger::instance();

        constexpr int kThreads = 8;
        constexpr int kIterations = 2000;
        std::atomic<int> done{0};

        std::vector<std::thread> ts;
        ts.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            ts.emplace_back([log, t, &done]() {
                // Half the threads write the level, half read+log; this
                // is the read/write contention pattern flagged in the
                // audit (UI-thread settings dialog vs. session worker
                // log calls).
                if (t % 2 == 0) {
                    for (int i = 0; i < kIterations; ++i) {
                        const LogLevel level = (i & 1) == 0
                            ? LogLevel::Debug
                            : LogLevel::Error;
                        log->setLogLevel(level);
                    }
                } else {
                    for (int i = 0; i < kIterations; ++i) {
                        log->info(QStringLiteral("concurrency probe"));
                    }
                }
                done.fetch_add(1, std::memory_order_relaxed);
            });
        }
        for (auto &th : ts) th.join();

        QCOMPARE(done.load(), kThreads);

        // logLevel() must return one of the values we stored — anything
        // else would indicate a torn read.
        const LogLevel finalLevel = log->logLevel();
        QVERIFY(finalLevel == LogLevel::Debug || finalLevel == LogLevel::Error);
    }

    void cleanupTestCase() {
        // Restore default to avoid affecting other tests in the same run.
        Logger::instance()->setLogLevel(LogLevel::Info);
        Logger::instance()->enableConsoleOutput(true);
    }
};

QTEST_MAIN(TestLogger)
#include "test_logger.moc"
