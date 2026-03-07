#include "../main_window.h"

void MainWindow::onReconnect() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size())
        return;
    Session *s = m_sessions[m_activeIndex];
    session::SessionConfig config = s->config;

    // Disconnect current session
    if (s->worker) {
        QMetaObject::invokeMethod(s->worker, "stop", Qt::QueuedConnection);
    }
    if (s->thread) {
        s->thread->quit();
        s->thread->wait(2000);
    }

    // Close the old tab and open a new one with the same config
    int oldIndex = m_activeIndex;
    onCloseTabRequested(oldIndex);
    connectToServer(config);
}
