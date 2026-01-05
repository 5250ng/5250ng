#include "session/config.h"
#include "session/manager.h"
#include "ui/main_window.h"
#include <QMessageBox>
#include <QString>

/**
 * Handle quick-open selection from the empty-state dropdown.
 *
 * Loads and connects to the selected saved session if valid.
 *
 * @param sessionName Name of the saved session chosen by the user.
 */
void MainWindow::onQuickOpenChanged(const QString &sessionName) {
    if (sessionName.isEmpty() || sessionName == "(Open saved session)") {
        return;
    }
    session::SessionManager mgr(this);
    session::SessionConfig cfg;
    if (mgr.loadSession(sessionName, cfg)) {
        connectToServer(cfg);
        // Reset selection to placeholder for next use
        // no reset needed for submenu
    } else {
        QMessageBox::warning(
            this, "Open Session",
            QString("Failed to open session '%1'.").arg(sessionName)
        );
    }
}
