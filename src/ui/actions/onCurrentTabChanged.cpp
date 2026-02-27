#include "../main_window.h"

void MainWindow::onCurrentTabChanged(int index) {
    setActiveSession(index);
}
