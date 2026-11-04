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

#include <QString>
#include <QtUiStyle/StyledMessageBox.h>

namespace ui {

// Whether the main window may close while connectedCount sessions are still
// connected: asks the user when there is at least one (issue #409)
inline bool confirmQuitWithConnectedSessions(QWidget *parent, int connectedCount) {
    if (connectedCount <= 0)
        return true;
    const QString text = connectedCount == 1
        ? QStringLiteral("1 session is connected. Quit anyway?")
        : QStringLiteral("%1 sessions are connected. Quit anyway?").arg(connectedCount);
    return qt_ui_style::StyledMessageBox::question(parent, QStringLiteral("Quit"), text) ==
           qt_ui_style::StyledMessageBox::Yes;
}

} // namespace ui
