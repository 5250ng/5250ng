#include "../main_window.h"

void MainWindow::onToggleHotspots() {
    if (m_displayWidget) {
        m_displayWidget->toggleHotspots();
        m_hotspotsAction->setChecked(m_displayWidget->hotspotsEnabled());
    }
}
