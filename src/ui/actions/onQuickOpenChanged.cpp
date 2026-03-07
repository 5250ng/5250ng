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
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QString>

/**
 * Handle quick-open selection from the empty-state dropdown.
 *
 * Loads and connects to the selected saved session if valid.
 *
 * @param sessionName Name of the saved session chosen by the user.
 */
void MainWindow::onQuickOpenChanged(const QString &sessionName) {
    if (sessionName.isEmpty() || sessionName == "(Open saved session)") {
        return;
    }
    session::SessionManager mgr(this);
    session::SessionConfig cfg;
    if (mgr.loadSession(sessionName, cfg)) {
        connectToServer(cfg);
        // Reset selection to placeholder for next use
        // no reset needed for submenu
    } else {
        ui::widgets::StyledMessageBox::warning(
            this, "Open Session",
            QString("Failed to open session '%1'.").arg(sessionName)
        );
    }
}
