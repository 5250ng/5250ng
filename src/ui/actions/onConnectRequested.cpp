#include "../main_window.h"
#include "session/config.h"

void MainWindow::onConnectRequested(const session::SessionConfig &config) {
    connectToServer(config);
}