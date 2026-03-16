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

void MainWindow::onCloseTabRequested(int index) {
    if (index < 0 || index >= m_sessions.size()) {
        return;
    }
    Session *s = m_sessions[index];
    // Disconnect all signals from worker to prevent queued signals from
    // firing after the session is deleted (use-after-free guard)
    if (s->worker) {
        s->worker->disconnect(this);
        QMetaObject::invokeMethod(s->worker, "stop", Qt::QueuedConnection);
    }
    // Disconnect cross-object signals within the session to prevent
    // queued signals from firing between objects during destruction
    if (s->displayWidget) {
        s->displayWidget->disconnect();
    }
    if (s->commandHandler) {
        s->commandHandler->disconnect();
    }
    if (s->parser) {
        s->parser->disconnect();
    }
    // Stop macro playback if running
    if (s->macroRecorder) {
        s->macroRecorder->stopPlayback();
        s->macroRecorder->disconnect();
    }
    // Stop script execution if running
    if (s->scriptExecutor) {
        s->scriptExecutor->stop();
        s->scriptExecutor->disconnect();
    }
    // Stop session logging
    if (s->sessionLogger && s->sessionLogger->isActive()) {
        s->sessionLogger->stop();
    }
    // Null out pointers before deletion so any stale queued lambdas see nullptr
    s->displayWidget = nullptr;
    s->parser = nullptr;
    s->connectionStatus = nullptr;
    s->coordinatesLabel = nullptr;
    s->kbdStateLabel = nullptr;
    s->systemNameLabel = nullptr;
    s->historyLabel = nullptr;
    s->agentPanel = nullptr;
    s->terminalContainer = nullptr;
    s->splitter = nullptr;
    if (s->thread) {
        s->thread->quit();
        s->thread->wait(2000);
        s->thread->deleteLater();
        s->thread = nullptr;
    }
    m_sessions.remove(index);
    m_tabWidget->removeTab(index);
    delete s; // deletes container and children (parser owned by container)
    if (!m_sessions.isEmpty()) {
        int newIndex = qMin(index, m_sessions.size() - 1);
        setActiveSession(newIndex);
    } else {
        setActiveSession(-1);
    }
    updateEmptyState();
}
