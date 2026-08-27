// 5250ng - A modern IBM TN5250 terminal emulator                                                                                                                                                            
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)                                                                                                                                                          
//                                                                                                                                                                                                           
// This program is free software: you can redistribute it and/or modify                                                                                                                                      
// it under the terms of the GNU General Public License as published by                                                                                                                                      
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.                                                                                                                                                                       
//                                                                                                                                                                                                           
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "../main_window.h"
#include <QDir>
#include <QFileDialog>
#include <QtUiStyle/StyledMessageBox.h>
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
        qt_ui_style::StyledMessageBox::information(this, "Screenshot", "No active session tab to capture.");
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
        qt_ui_style::StyledMessageBox::warning(this, "Screenshot", "Failed to save screenshot to the selected file.");
    }
}