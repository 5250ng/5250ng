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
#include "ui/dialogs/match_replace_dialog.h"

void MainWindow::onToggleMatchReplace() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->matchReplace) {
        s->matchReplace = new core::MatchReplaceEngine(s->container);
    }

    bool newState = !s->matchReplace->isEnabled();
    s->matchReplace->setEnabled(newState);
    m_matchReplaceEnableAction->setChecked(newState);
    s->displayWidget->setMatchReplaceEngine(s->matchReplace);
    // Rebuild overlay immediately so the current screen reflects the change
    s->displayWidget->refreshMatchReplaceOverlay();
    s->displayWidget->update();
}

void MainWindow::onEditMatchReplacePatterns() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->matchReplace) {
        s->matchReplace = new core::MatchReplaceEngine(s->container);
    }

    ui::dialogs::MatchReplaceDialog dialog(s->matchReplace->rules(), this);
    if (dialog.exec() == QDialog::Accepted) {
        s->matchReplace->setRules(dialog.rules());
        s->displayWidget->setMatchReplaceEngine(s->matchReplace);
        // Rebuild overlay immediately so the current screen reflects the new rules
        s->displayWidget->refreshMatchReplaceOverlay();
        s->displayWidget->update();
    }
}
