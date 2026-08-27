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

#include <QtUiStyle/BaseFramelessDialog.h>
#include <QString>

namespace ui::dialogs {

// Modal dialog shown for every host-issued STRPCCMD when the feature is
// enabled. Displays the host name and the full command string with a clear
// security warning, and offers Allow / Deny. Allow is NOT the default button
// — the user must explicitly choose it. Renders with the project's frameless
// title bar in a wide horizontal-rectangle layout matching the rest of the
// dialogs in src/ui/dialogs/.
class PcCommandConfirmDialog : public qt_ui_style::BaseFramelessDialog {
    Q_OBJECT

  public:
    PcCommandConfirmDialog(const QString &hostname,
                           const QString &command,
                           QWidget *parent = nullptr);

    // Shows the dialog modally and returns true if the user clicked Allow.
    static bool ask(const QString &hostname,
                    const QString &command,
                    QWidget *parent);

    // Notify the user that a host attempted to run a PC command that was
    // refused by policy ("Deny and alert" mode). Modeless / dismiss-only —
    // there is no Allow option, just an OK button.
    static void notifyDenied(const QString &hostname,
                             const QString &command,
                             QWidget *parent);
};

} // namespace ui::dialogs
