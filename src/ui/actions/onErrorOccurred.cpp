#include "../main_window.h"
#include <QMessageBox>

using namespace core;

void MainWindow::onErrorOccurred(const QString &error) {
    logger::Logger::instance()->error(QString("Connection error: %1").arg(error));
    QMessageBox::warning(this, "Connection Error", error);
    m_connected = false;
    m_connectAction->setEnabled(true);
    m_disconnectAction->setEnabled(false);
    // Status indicator will be updated via stateChanged signal
}