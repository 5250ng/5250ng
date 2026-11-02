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

class QLineEdit;

namespace ui::dialogs {

// Modal password prompt shown when an MCP client calls login without a
// password. The user types the password here; 5250ng types it into the
// sign-on screen itself, so it is never sent to or returned by the MCP
// client. Cancel is the safe default and the dialog rejects itself when the
// timeout expires.
class McpPasswordPromptDialog : public qt_ui_style::BaseFramelessDialog {
    Q_OBJECT

  public:
    McpPasswordPromptDialog(const QString &hostname,
                            const QString &username,
                            QWidget *parent = nullptr);

    QString password() const;

    // Shows the dialog modally. Returns true and fills *password when the
    // user clicks Sign on; false on Cancel or after timeoutMs.
    static bool ask(const QString &hostname,
                    const QString &username,
                    QString *password,
                    QWidget *parent,
                    int timeoutMs);

  private:
    QLineEdit *m_passwordEdit = nullptr;
};

} // namespace ui::dialogs
