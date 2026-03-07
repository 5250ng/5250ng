#include "../main_window.h"

void MainWindow::onEditCopy() {
    if (m_displayWidget)
        m_displayWidget->copySelection();
}

void MainWindow::onEditPaste() {
    if (m_displayWidget)
        m_displayWidget->handlePaste();
}

void MainWindow::onEditSelectAll() {
    if (m_displayWidget)
        m_displayWidget->selectAll();
}
