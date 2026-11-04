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

void MainWindow::onDuplicateSession() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size())
        return;
    // The configured device name, not the one this tab was given to avoid a
    // clash; the new tab gets its own unique name
    session::SessionConfig config = savedConfig(m_sessions[m_activeIndex]);
    // The copy is the original tab's, so is the profile snapshot it is from
    m_inheritProfileSnapshotOnce = m_sessions[m_activeIndex]->profileSnapshot;
    connectToServer(config);
}
