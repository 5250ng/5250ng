#include "../main_window.h"

void MainWindow::onLogMessage(logger::LogLevel level, const QString &message) {
    // Update status bar for important messages
    if (level == logger::LogLevel::Error) {
        statusBar()->showMessage(message, 5000);
    }
}