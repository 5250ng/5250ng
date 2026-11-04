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

#include <QRegularExpression>
#include <QString>
#include <climits>

namespace mcp {

// The time an MCP script call waits for its script: a 60 s base plus every
// WAIT and GLOBAL EXPECT_TIMEOUT value in it. The sum saturates at INT_MAX
// milliseconds, the most a QTimer takes: a plain int sum overflowed for a
// huge value (undefined behaviour, in practice a negative interval QTimer
// refuses, leaving the call without any timeout), and values too long for
// an int would otherwise parse as 0.
inline int scriptTimeoutMs(const QString &script) {
    static const QRegularExpression waitRe(
        R"(\bWAIT\s+(\d+))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression expectTimeoutRe(
        R"(\bGLOBAL\s+EXPECT_TIMEOUT\s+(\d+))", QRegularExpression::CaseInsensitiveOption);

    qint64 totalMs = 60000; // 60-second base
    auto add = [&totalMs](const QString &digits) {
        bool ok = false;
        const qint64 v = digits.toLongLong(&ok);
        totalMs += ok ? qMin<qint64>(v, INT_MAX) : qint64(INT_MAX);
        totalMs = qMin<qint64>(totalMs, INT_MAX);
    };
    for (const QRegularExpression *re : {&waitRe, &expectTimeoutRe}) {
        auto it = re->globalMatch(script);
        while (it.hasNext())
            add(it.next().captured(1));
    }
    return int(totalMs);
}

} // namespace mcp
