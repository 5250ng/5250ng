#include "../main_window.h"

void MainWindow::onDisconnect() {
    if (m_activeIndex >= 0 && m_activeIndex < m_sessions.size()) {
        Session *s = m_sessions[m_activeIndex];
        if (s && s->worker) {
            QMetaObject::invokeMethod(s->worker, "stop", Qt::QueuedConnection);
        }
    }
}
