#include "../main_window.h"

void MainWindow::onDisconnect() {
    if (m_client) {
        m_client->disconnectFromHost();
    }
}