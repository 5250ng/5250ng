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
#include "ui/main_window.h"
#include <QAction>
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QString>

/**
 * Open a saved session chosen from the "Open saved session" submenu.
 *
 * @param action The triggered action carrying the session name via data().
 */
void MainWindow::onSavedSessionChosen(QAction *action) {
    if (!action)
        return;
    QString sessionName = action->data().toString();
    if (sessionName.isEmpty())
        return;
    session::SessionManager mgr(this);
    session::SessionConfig cfg;
    if (mgr.loadSession(sessionName, cfg)) {
        connectToServer(cfg);
    } else {
        ui::widgets::StyledMessageBox::warning(
            this, "Open Session",
            QString("Failed to open session '%1'.").arg(sessionName)
        );
    }
}
