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
    // Stop macro playback if running
    if (s->macroRecorder) {
        s->macroRecorder->stopPlayback();
        s->macroRecorder->disconnect();
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
