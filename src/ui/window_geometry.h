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

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QWidget>

namespace ui {

// QSettings key of the main window's size and position (issue #412)
inline const QString kMainWindowGeometryKey = QStringLiteral("mainWindow/geometry");

// Saves the window's geometry under key. A full-screen window is not saved,
// so the next launch opens at the last normal or maximized geometry.
inline void saveWindowGeometry(const QWidget *window, const QString &key) {
    if (window->isFullScreen())
        return;
    QSettings().setValue(key, window->saveGeometry());
}

// Applies the geometry saved under key, if any. Qt moves a geometry that is
// off every current screen back onto one. Returns whether one was applied.
inline bool restoreWindowGeometry(QWidget *window, const QString &key) {
    const QByteArray geometry = QSettings().value(key).toByteArray();
    return !geometry.isEmpty() && window->restoreGeometry(geometry);
}

} // namespace ui
