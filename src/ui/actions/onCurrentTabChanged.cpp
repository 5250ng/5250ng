#include "../main_window.h"

using namespace core;

void MainWindow::onCurrentTabChanged(int index) {
    disconnectSessionSignals();
    setActiveSession(index);
    connectSessionSignals();
}
