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

void MainWindow::onCurrentTabChanged(int index) {
    setActiveSession(index);
    // Re-route the virtual keyboard pulse feed onto the new active session so
    // the on-screen keyboard keeps highlighting the right host chord.
    rebindVirtualKeyboardPulse();
    if (index >= 0 && index < m_sessions.size()) {
        Session *s = m_sessions[index];
        // Clear activity indicator when the tab gains focus
        if (s->hasActivity) {
            s->hasActivity = false;
            QVariantMap td = m_tabWidget->tabBar()->tabData(index).toMap();
            td["activity"] = false;
            m_tabWidget->tabBar()->setTabData(index, td);
            m_tabWidget->tabBar()->update();
        }
        // The per-session menu items follow the new tab (setActiveSession()
        // synced them)
    }
}
