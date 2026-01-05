#include "session/config.h"
#include "session/manager.h"
#include "ui/main_window.h"
#include <QAction>
#include <QMessageBox>
#include <QString>

/**
 * Open a saved session chosen from the "Open saved session" submenu.
 *
 * @param action The triggered action carrying the session name via data().
 */
void MainWindow::onSavedSessionChosen(QAction *action) {
    if (!action)
        return;
    QString sessionName = action->data().toString();
    if (sessionName.isEmpty())
        return;
    session::SessionManager mgr(this);
    session::SessionConfig cfg;
    if (mgr.loadSession(sessionName, cfg)) {
        connectToServer(cfg);
    } else {
        QMessageBox::warning(
            this, "Open Session",
            QString("Failed to open session '%1'.").arg(sessionName)
        );
    }
}
