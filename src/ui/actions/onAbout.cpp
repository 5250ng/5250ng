#include "../main_window.h"
#include <QMessageBox>

using namespace core;

void MainWindow::onAbout() {
    QMessageBox::about(
        this, "About 5250ng",
        "5250ng - A modern TN5250 client\n\n"
        "Multi-session tabbed interface with TN5250 protocol support.\n"
        "Built with Qt6."
    );
}