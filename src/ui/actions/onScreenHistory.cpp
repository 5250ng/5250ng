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

#include "../main_window.h"

void MainWindow::onHistoryBack() {
    if (!m_displayWidget) return;
    int idx = m_displayWidget->historyIndex();
    int count = m_displayWidget->screenHistory()->count();
    if (count == 0) return;

    if (idx < 0) {
        // Currently on live screen, go to most recent history
        m_displayWidget->viewHistoryScreen(0);
    } else if (idx + 1 < count) {
        m_displayWidget->viewHistoryScreen(idx + 1);
    }
}

void MainWindow::onHistoryForward() {
    if (!m_displayWidget) return;
    int idx = m_displayWidget->historyIndex();
    if (idx < 0) return; // Already on live screen

    if (idx == 0) {
        // Return to live screen
        m_displayWidget->exitHistoryView();
    } else {
        m_displayWidget->viewHistoryScreen(idx - 1);
    }
}

void MainWindow::onHistoryExit() {
    if (m_displayWidget && m_displayWidget->isViewingHistory()) {
        m_displayWidget->exitHistoryView();
    }
}
