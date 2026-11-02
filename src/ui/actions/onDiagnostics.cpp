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
#include "ui/dialogs/log_viewer.h"
#include "logger/logger.h"
#include <QDesktopServices>
#include <QDir>
#include <QUrl>

/**
 * Help > Enable Debug Logging: switch the application log between Info and
 * Debug level. Debug level also writes the log to Logger::defaultLogFilePath().
 * The menu state and saved setting follow Logger::debugLoggingChanged.
 */
void MainWindow::onToggleDebugLogging() {
    logger::Logger::instance()->setDebugLogging(m_debugLoggingAction->isChecked());
}

/**
 * Help > Save Diagnostic Log: save the buffered application log, with an
 * environment header, to a file the user can attach to a bug report.
 */
void MainWindow::onSaveDiagnosticLog() {
    LogViewerDialog::saveDiagnosticLog(this);
}

/**
 * Help > Show Log Folder: open the directory holding the log file in the
 * platform file manager (Finder, Explorer, ...).
 */
void MainWindow::onShowLogFolder() {
    const QString dir = logger::Logger::logDirectory();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
