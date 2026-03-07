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
#include <QTabWidget>
#include <QtGlobal>

using namespace core;

void MainWindow::onTabMoved(int from, int to) {
    if (from < 0 || from >= m_sessions.size() || to < 0 ||
        to >= m_sessions.size()) {
        return;
    }
    m_sessions.move(from, to);
    if (m_activeIndex == from) {
        m_activeIndex = to;
    } else if (m_activeIndex >= qMin(from, to) &&
               m_activeIndex <= qMax(from, to)) {
        // Active index may shift by one depending on move direction
        // Simplest is to refresh active session pointers
        setActiveSession(m_tabWidget->currentIndex());
    }
}
