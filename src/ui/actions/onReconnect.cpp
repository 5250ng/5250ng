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
    // The reopened tab holds the same copy, so it keeps the same snapshot:
    // an edit to the profile made meanwhile is still noticed on Save
    const QByteArray snapshot = s->profileSnapshot;

    // Close the old tab and open a new one with the same config. Closing the
    // tab stops the worker and its thread; doing that here first would let
    // the finished thread delete the worker, which onCloseTabRequested()
    // then still uses.
    int oldIndex = m_activeIndex;
    // The new tab takes this one's place and label, keeps a running log
    // going, and keeps an MCP client's session id (#470)
    ReopenedTab reopen;
    reopen.index = oldIndex;
    reopen.label = m_tabWidget->tabText(oldIndex);
    if (s->sessionLogger && s->sessionLogger->isActive()) {
        reopen.logPath = s->sessionLogger->filePath();
        reopen.logVerbosity = s->sessionLogger->verbosity();
    }
    const QString mcpSessionId = s->mcpControlled ? s->mcpSessionId : QString();
    m_closingForReconnect = s;
    onCloseTabRequested(oldIndex);
    m_closingForReconnect = nullptr;
    m_skipStartupScriptOnce = !config.startupScriptOnReconnect();
    m_inheritProfileSnapshotOnce = snapshot;
    m_reopenTabOnce = reopen;
    connectToServer(config, mcpSessionId);
}
