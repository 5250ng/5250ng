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

void MainWindow::onReconnect() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size())
        return;
    Session *s = m_sessions[m_activeIndex];
    // With the configured device name, so the reopened tab still knows it
    // (the de-duplication picks a free name again)
    session::SessionConfig config = savedConfig(s);

    // Close the old tab and open a new one with the same config. Closing the
    // tab stops the worker and its thread; doing that here first would let
    // the finished thread delete the worker, which onCloseTabRequested()
    // then still uses.
    int oldIndex = m_activeIndex;
    onCloseTabRequested(oldIndex);
    m_skipStartupScriptOnce = !config.startupScriptOnReconnect();
    connectToServer(config);
}
