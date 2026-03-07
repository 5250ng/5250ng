#include "../main_window.h"
#include <QLabel>
#include <QStatusBar>

void MainWindow::setupStatusBar() {
    // Status bar is available for transient messages via statusBar()->showMessage()
    // Per-session indicators (connection status, cursor coordinates) live in each tab's footer
}
