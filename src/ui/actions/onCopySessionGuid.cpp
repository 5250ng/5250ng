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
#include <QApplication>
#include <QClipboard>

void MainWindow::onCopySessionGuid() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size())
        return;

    QString guid = m_sessions[m_activeIndex]->guid;
    QApplication::clipboard()->setText(guid);

    ui::widgets::StyledMessageBox::information(
        this, "Session GUID",
        QString("Session GUID copied to clipboard:\n%1").arg(guid));
}
