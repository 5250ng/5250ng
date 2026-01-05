#include "../main_window.h"

using namespace core;

void MainWindow::onCloseTabRequested(int index) {
    if (index < 0 || index >= m_sessions.size()) {
        return;
    }
    Session *s = m_sessions[index];
    // Stop per-session worker/thread if present
    if (s->worker) {
        QMetaObject::invokeMethod(s->worker, "stop", Qt::QueuedConnection);
    }
    if (s->thread) {
        s->thread->quit();
        s->thread->wait(2000);
        s->thread->deleteLater();
        s->thread = nullptr;
    }
    if (index == m_activeIndex) {
        disconnectSessionSignals();
    }
    m_sessions.remove(index);
    m_tabWidget->removeTab(index);
    delete s; // deletes container and children (client/parser owned by container)
    if (!m_sessions.isEmpty()) {
        int newIndex = qMin(index, m_sessions.size() - 1);
        setActiveSession(newIndex);
        connectSessionSignals();
    } else {
        setActiveSession(-1);
    }
    updateEmptyState();
}