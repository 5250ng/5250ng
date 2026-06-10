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
#include <QFileDialog>
#include <QFileInfo>

// ---------------------------------------------------------------------------
// Replay Session from PCAP (issue #30)
//
// Opens a regular session tab whose Worker, instead of connecting a socket,
// feeds the host→client TN5250 records extracted from a capture file through
// the normal decoder → renderer pipeline. The tab is read-only: there is no
// peer to answer input.
// ---------------------------------------------------------------------------

void MainWindow::onReplayPcap() {
    QString filePath = QFileDialog::getOpenFileName(this, "Replay Session from PCAP",
        QString(), "Capture files (*.pcap *.pcapng *.cap);;All files (*)");
    if (filePath.isEmpty()) return;
    startPcapReplay(filePath);
}

void MainWindow::startPcapReplay(const QString &path) {
    QFileInfo fi(path);
    session::SessionConfig config;
    config.setName(fi.fileName());
    // Hostname is display-only for replay sessions (tab fallback label and
    // SessionConfig::isValid); no connection is ever opened to it.
    config.setHostname(fi.fileName());
    config.setReplayPcapFile(fi.absoluteFilePath());
    connectToServer(config);
}
