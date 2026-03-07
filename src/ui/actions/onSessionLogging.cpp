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
#include "ui/widgets/Frameless/StyledMessageBox.h"
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

void MainWindow::onToggleSessionLogging() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;
    Session *s = m_sessions[m_activeIndex];

    if (!s->sessionLogger) {
        s->sessionLogger = new core::SessionLogger(s->container);
    }

    if (s->sessionLogger->isActive()) {
        s->sessionLogger->stop();
        m_sessionLoggingAction->setChecked(false);
        ui::widgets::StyledMessageBox::information(this, "Session Logging",
            QString("Logging stopped.\nLog saved to: %1").arg(s->sessionLogger->filePath()));
    } else {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      + "/logs";
        QDir().mkpath(dir);
        QString filename = QString("session_%1_%2.log")
            .arg(s->config.hostname().replace('.', '_'))
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        QString path = dir + "/" + filename;

        if (s->sessionLogger->start(path)) {
            m_sessionLoggingAction->setChecked(true);
            s->sessionLogger->logEvent(
                QString("Connected to %1:%2").arg(s->config.hostname()).arg(s->config.port()));
        } else {
            ui::widgets::StyledMessageBox::warning(this, "Session Logging",
                QString("Failed to open log file:\n%1").arg(path));
        }
    }
}
