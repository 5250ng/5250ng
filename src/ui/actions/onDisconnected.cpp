#include "../main_window.h"

using namespace core;

void MainWindow::onDisconnected() {
    logger::Logger::instance()->debug("[TN5250->UI]: Disconnected from TN5250 server");
    m_connected = false;
    m_connectAction->setEnabled(true);
    m_disconnectAction->setEnabled(false);
    // Status indicator and text will be updated via stateChanged signal
}