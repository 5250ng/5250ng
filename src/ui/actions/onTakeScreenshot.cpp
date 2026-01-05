#include "../main_window.h"
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QWidget>

using namespace core;

/**
 * Capture the visible contents of the current session tab as a PNG.
 *
 * Prompts the user for a destination file and saves a snapshot of the tab.
 */
void MainWindow::onTakeScreenshot() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) {
        QMessageBox::information(this, "Screenshot", "No active session tab to capture.");
        return;
    }
    QString defaultPath = QDir::homePath() + "/tn5250ng-screenshot.png";
    QString fileName = QFileDialog::getSaveFileName(
        this, "Save Screenshot", defaultPath, "PNG Images (*.png)"
    );
    if (fileName.isEmpty()) {
        return;
    }
    QWidget *tabWidgetContainer = m_sessions[m_activeIndex]->container;
    QPixmap pixmap = tabWidgetContainer->grab();
    if (!pixmap.save(fileName, "PNG")) {
        QMessageBox::warning(this, "Screenshot", "Failed to save screenshot to the selected file.");
    }
}