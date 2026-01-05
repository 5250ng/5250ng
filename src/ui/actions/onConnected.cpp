#include "../main_window.h"
#include "logger/logger.h"

void MainWindow::onConnected() {
    logger::Logger::instance()->debug("[TN5250->UI]: Connected to TN5250 server");
    m_connected = true;
    m_connectAction->setEnabled(false);
    m_disconnectAction->setEnabled(true);
    if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
        Session *s = m_sessions[m_activeIndex];
        if (s && s->connectionStatus) {
            s->connectionStatus->setStatusText(QString("Connected to %1:%2")
                                                   .arg(m_currentSession.hostname())
                                                   .arg(m_currentSession.port()));
        }
    }
    // Status indicator will be updated via stateChanged signal
}