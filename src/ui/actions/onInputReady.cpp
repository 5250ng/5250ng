#include "../main_window.h"

using namespace core;

void MainWindow::onInputReady(const QByteArray &data) {
    if (m_client && m_client->isConnected()) {
        m_client->sendData(data);
        logger::Logger::instance()->debug(
            QString("Data sent: %1 bytes").arg(data.size())
        );
    }
}
