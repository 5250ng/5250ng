#include "../main_window.h"

void MainWindow::onHistoryBack() {
    if (!m_displayWidget) return;
    int idx = m_displayWidget->historyIndex();
    int count = m_displayWidget->screenHistory()->count();
    if (count == 0) return;

    if (idx < 0) {
        // Currently on live screen, go to most recent history
        m_displayWidget->viewHistoryScreen(0);
    } else if (idx + 1 < count) {
        m_displayWidget->viewHistoryScreen(idx + 1);
    }
}

void MainWindow::onHistoryForward() {
    if (!m_displayWidget) return;
    int idx = m_displayWidget->historyIndex();
    if (idx < 0) return; // Already on live screen

    if (idx == 0) {
        // Return to live screen
        m_displayWidget->exitHistoryView();
    } else {
        m_displayWidget->viewHistoryScreen(idx - 1);
    }
}

void MainWindow::onHistoryExit() {
    if (m_displayWidget && m_displayWidget->isViewingHistory()) {
        m_displayWidget->exitHistoryView();
    }
}
