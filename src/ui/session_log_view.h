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

#pragma once

#include "core/session_logger.h"
#include "ui/dialogs/log_viewer.h"
#include <QtUiStyle/StyledMessageBox.h>
#include <QObject>
#include <QWidget>

namespace ui {

// Session > View Session Logs: open the tab's own session log, followed
// while it grows, or explain how to start one. Returns the window shown,
// or nullptr when the tab has no log file yet.
inline LogViewerDialog *showSessionLog(QWidget *parent, const core::SessionLogger *logger,
                                       const QString &tabName) {
    const QString path = logger ? logger->filePath() : QString();
    if (path.isEmpty()) {
        qt_ui_style::StyledMessageBox::information(
            parent, QObject::tr("Session Log"),
            QObject::tr("Session logging is off for this tab.\n\n"
                        "Turn it on with Tools > Session Logging; this window then shows "
                        "the log as it is written. Help > View Logs shows the application log."));
        return nullptr;
    }
    const QString title = logger->isActive()
        ? QObject::tr("Session Log - %1").arg(tabName)
        : QObject::tr("Session Log - %1 (stopped)").arg(tabName);
    auto *viewer = new LogViewerDialog(path, title, parent);
    viewer->setAttribute(Qt::WA_DeleteOnClose, true);
    viewer->show();
    return viewer;
}

} // namespace ui
