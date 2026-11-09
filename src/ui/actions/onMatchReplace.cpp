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
#include "session/manager.h"
#include "ui/dialogs/match_replace_dialog.h"

void MainWindow::applySavedMatchReplace(Session *s) {
    const auto &rules = s->config.matchReplaceRules();
    if (rules.isEmpty() && !s->config.matchReplaceEnabled()) return;
    if (!s->matchReplace)
        s->matchReplace = new core::MatchReplaceEngine(s->container);
    s->matchReplace->setRules(rules);
    s->matchReplace->setEnabled(s->config.matchReplaceEnabled());
    s->displayWidget->setMatchReplaceEngine(s->matchReplace);
    s->displayWidget->refreshMatchReplaceOverlay();
}

// The rules and toggle are this tab's own: keep them in its config, so Save
// writes them, and in its saved profile right away, as the quick theme does
void MainWindow::persistMatchReplace(Session *s) {
    s->config.setMatchReplaceRules(s->matchReplace->rules());
    s->config.setMatchReplaceEnabled(s->matchReplace->isEnabled());
    const QString name = s->config.name();
    if (name.isEmpty() || name == "Current Session" || name == "Command Line Session"
        || !s->config.replayPcapFile().isEmpty())
        return;
    // Save need not warn about this write, but only if the profile had not
    // been edited elsewhere before: that edit is still not in this tab's copy
    const bool snapshotCurrent = savedProfileJson(s) == s->profileSnapshot;
    session::SessionManager mgr;
    session::SessionConfig saved;
    if (!mgr.loadSession(name, saved)) return;
    saved.setMatchReplaceRules(s->config.matchReplaceRules());
    saved.setMatchReplaceEnabled(s->config.matchReplaceEnabled());
    mgr.saveSession(saved);
    if (snapshotCurrent)
        s->profileSnapshot = savedProfileJson(s);
}

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
    persistMatchReplace(s);
}

void MainWindow::onEditMatchReplacePatterns() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->matchReplace) {
        s->matchReplace = new core::MatchReplaceEngine(s->container);
    }

    ui::dialogs::MatchReplaceDialog dialog(s->matchReplace->rules(), this);
    if (dialog.exec() != QDialog::Accepted) return;

    // Session may have been closed while the dialog was open
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()
        || m_sessions[m_activeIndex] != s) {
        return;
    }

    {
        s->matchReplace->setRules(dialog.rules());
        s->displayWidget->setMatchReplaceEngine(s->matchReplace);
        // Rebuild overlay immediately so the current screen reflects the new rules
        s->displayWidget->refreshMatchReplaceOverlay();
        s->displayWidget->update();
        persistMatchReplace(s);
    }
}
