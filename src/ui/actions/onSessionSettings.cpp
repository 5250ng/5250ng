#include "session/config.h"
#include "ui/main_window.h"
#include <QDialog>
#include <QMessageBox>
#include <QString>
#include <QWidget>

/**
 * Open the session settings dialog for the active tab.
 *
 * Lets the user adjust device name and display size, and applies the changes
 * to the active session and tab title.
 */
void MainWindow::onSessionSettings() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) {
        QMessageBox::information(this, "Session Settings", "No active session to configure.");
        return;
    }
    Session *s = m_sessions[m_activeIndex];
    ConnectDialog dlg(this);
    dlg.setSessionConfig(m_currentSession);
    if (dlg.exec() == QDialog::Accepted) {
        session::SessionConfig newConfig = dlg.getSessionConfig();
        // Apply settings to active session
        s->config = newConfig;
        m_currentSession = newConfig;
        if (s->displayWidget) {
            s->displayWidget->setScreenSize(newConfig.screenRows(), newConfig.screenCols());
            updateCursorCoordinatesFont();
            updateCursorCoordinates();
        }
        QString tabText =
            !newConfig.name().isEmpty()
                ? newConfig.name()
                : QString("%1:%2").arg(newConfig.hostname()).arg(newConfig.port());
        m_tabWidget->setTabText(m_activeIndex, tabText);
    }
}
