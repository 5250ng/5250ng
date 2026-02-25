#include "../main_window.h"

using namespace core;

void MainWindow::onInputReady(const QByteArray &data) {
    sendToHost(data);
    logger::Logger::instance()->debug(
        QString("Data sent via worker: %1 bytes").arg(data.size())
    );
}
