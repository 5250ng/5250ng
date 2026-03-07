#include "../main_window.h"

void MainWindow::onToggleFullscreen() {
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
    m_fullscreenAction->setChecked(isFullScreen());
}
