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
#include <QStringList>

namespace core {

/// The system name shown on an IBM i sign-on screen ("System . . . . . :
/// RSRCH09"), or an empty string if the screen is not a sign-on screen.
/// System names are up to 8 characters, A-Z, 0-9, $, # and @, not starting
/// with a digit.
/// Only the first rows are searched; the label is the English one.
inline QString signOnSystemName(const QStringList &rows) {
    static const QRegularExpression re(
        QStringLiteral(R"(\bSystem(?:\s+\.)+\s*:\s+([A-Z$#@][A-Z0-9$#@]{0,7})(?![A-Z0-9$#@_]))"));
    const int last = qMin(int(rows.size()), 8);
    for (int i = 0; i < last; ++i) {
        const auto match = re.match(rows.at(i));
        if (match.hasMatch())
            return match.captured(1);
    }
    return QString();
}

} // namespace core
