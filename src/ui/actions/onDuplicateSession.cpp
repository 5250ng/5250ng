#include "../main_window.h"

void MainWindow::onDuplicateSession() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size())
        return;
    session::SessionConfig config = m_sessions[m_activeIndex]->config;
    connectToServer(config);
}
