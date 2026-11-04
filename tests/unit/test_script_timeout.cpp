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

#include "mcp/script_timeout.h"
#include <QtTest/QtTest>
#include <climits>

using mcp::scriptTimeoutMs;

// Regression tests for issue #405: the MCP script timeout summed WAIT and
// EXPECT_TIMEOUT values in a plain int, which overflowed for large values.
class TestScriptTimeout : public QObject {
    Q_OBJECT

  private slots:
    void baseAndSum() {
        QCOMPARE(scriptTimeoutMs(QStringLiteral("PRESS ENTER")), 60000);
        QCOMPARE(scriptTimeoutMs(QStringLiteral("WAIT 500\nWAIT 1500\nGLOBAL EXPECT_TIMEOUT 2000")),
                 64000);
    }

    void hugeValuesSaturate() {
        QCOMPARE(scriptTimeoutMs(QStringLiteral("WAIT 2147483647")), INT_MAX);
        QCOMPARE(scriptTimeoutMs(QStringLiteral("WAIT 2000000000\nWAIT 2000000000")), INT_MAX);
        QCOMPARE(scriptTimeoutMs(QStringLiteral("GLOBAL EXPECT_TIMEOUT 2147483647")), INT_MAX);
    }

    void valuesTooLongForAnIntSaturate() {
        // These used to parse as 0
        QCOMPARE(scriptTimeoutMs(QStringLiteral("WAIT 99999999999999999999")), INT_MAX);
    }

    void neverNegative() {
        QVERIFY(scriptTimeoutMs(QStringLiteral("WAIT 2147423648")) > 0);
        QVERIFY(scriptTimeoutMs(QStringLiteral("WAIT 2147483647\nGLOBAL EXPECT_TIMEOUT 2147483647")) > 0);
    }
};

QTEST_MAIN(TestScriptTimeout)
#include "test_script_timeout.moc"
