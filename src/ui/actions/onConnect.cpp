#include "../main_window.h"
#include "session/config.h"

void MainWindow::onConnect() {
    ConnectDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        session::SessionConfig config = dialog.getSessionConfig();
        connectToServer(config);
    }
}
