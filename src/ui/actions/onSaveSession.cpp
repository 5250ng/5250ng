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
#include "session/config.h"
#include "session/manager.h"
#include <QtUiStyle/StyledMessageBox.h>

/**
 * Save the active tab's configuration back to its saved session, or prompt
 * for a name when the tab was not opened from a saved session.
 */
void MainWindow::onSaveSession() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size())
        return;
    Session *s = m_sessions[m_activeIndex];
    if (!s->config.replayPcapFile().isEmpty()) {
        qt_ui_style::StyledMessageBox::information(
            this, "Save Session", "PCAP replay sessions cannot be saved.");
        return;
    }

    session::SessionManager mgr(this);
    if (!mgr.sessionExists(s->config.name())) {
        onSaveSessionAs();
        return;
    }
    if (!mgr.saveSession(s->config)) {
        qt_ui_style::StyledMessageBox::warning(this, "Save Session", "Failed to save session.");
        return;
    }
    qt_ui_style::StyledMessageBox::information(
        this, "Save Session",
        QString("Session '%1' saved successfully.").arg(s->config.name()));
}

/**
 * Save the active tab's configuration under a new name.
 */
void MainWindow::onSaveSessionAs() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size())
        return;
    Session *s = m_sessions[m_activeIndex];
    if (!s->config.replayPcapFile().isEmpty()) {
        qt_ui_style::StyledMessageBox::information(
            this, "Save Session", "PCAP replay sessions cannot be saved.");
        return;
    }

    session::SessionConfig config = s->config;
    if (!ConnectDialog::promptSaveSessionAs(this, config))
        return;

    // The tab now belongs to the saved session it was written to
    s->config.setName(config.name());
    m_currentSession.setName(config.name());
    m_tabWidget->setTabText(m_activeIndex, config.name());
    if (m_emptyOpenCombo)
        fillSessionsCombo(m_emptyOpenCombo, "(Open saved session)");
    qt_ui_style::StyledMessageBox::information(
        this, "Save Session",
        QString("Session '%1' saved successfully.").arg(config.name()));
}
