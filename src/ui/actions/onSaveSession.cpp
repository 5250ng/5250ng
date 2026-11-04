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
#include <QJsonDocument>

/**
 * The tab's configuration as it should be saved: without runtime-only
 * adjustments such as the de-duplicated device name.
 */
session::SessionConfig MainWindow::savedConfig(const Session *s) {
    session::SessionConfig config = s->config;
    config.setDeviceName(s->requestedDeviceName);
    return config;
}

QByteArray MainWindow::savedProfileJson(const Session *s) {
    session::SessionManager mgr;
    session::SessionConfig saved;
    if (s->config.name().isEmpty() || !mgr.loadSession(s->config.name(), saved))
        return {};
    return QJsonDocument(saved.toJson()).toJson(QJsonDocument::Compact);
}

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
    // The profile may have been edited (Connect dialog, another tab) since
    // this tab read it; saving writes the tab's whole copy over those edits
    if (savedProfileJson(s) != s->profileSnapshot) {
        auto answer = qt_ui_style::StyledMessageBox::question(
            this, "Save Session",
            QString("The saved session '%1' was changed since this tab opened it.\n\n"
                    "Replace it with this tab's settings? The other changes will be lost.")
                .arg(s->config.name()));
        if (answer != qt_ui_style::StyledMessageBox::Yes)
            return;
    }
    if (!mgr.saveSession(savedConfig(s))) {
        qt_ui_style::StyledMessageBox::warning(this, "Save Session", "Failed to save session.");
        return;
    }
    s->profileSnapshot = savedProfileJson(s);
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

    session::SessionConfig config = savedConfig(s);
    if (!ConnectDialog::promptSaveSessionAs(this, config))
        return;

    // The tab now belongs to the saved session it was written to
    s->config.setName(config.name());
    s->profileSnapshot = savedProfileJson(s);
    m_currentSession.setName(config.name());
    m_tabWidget->setTabText(m_activeIndex, config.name());
    if (m_emptyOpenCombo)
        fillSessionsCombo(m_emptyOpenCombo, "(Open saved session)");
    qt_ui_style::StyledMessageBox::information(
        this, "Save Session",
        QString("Session '%1' saved successfully.").arg(config.name()));
}
