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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace core {

/// Makes the application's settings file readable by its owner only. It
/// holds AI provider API keys and OAuth tokens, and is otherwise created
/// with the umask's default (typically world-readable). QSettings rewrites
/// the file through QSaveFile, which keeps the permissions of the file it
/// replaces, so restricting it once covers later writes by any part of the
/// application. On Windows the settings are in the user's registry hive; on
/// macOS the native store is CFPreferences under the user-private ~/Library,
/// where this is best effort.
inline void restrictSettingsToOwner(QSettings &settings) {
#ifndef Q_OS_WIN
    const QString path = settings.fileName();
    const QFileDevice::Permissions ownerOnly = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    // A file that does not exist yet is created owner-only first, so the
    // first write (which keeps that mode) never leaves it world-readable.
    // Not for macOS's native store: CFPreferences owns that plist.
#ifdef Q_OS_MACOS
    const bool iniFile = settings.format() != QSettings::NativeFormat;
#else
    const bool iniFile = true;
#endif
    if (iniFile && !QFile::exists(path) && QDir().mkpath(QFileInfo(path).absolutePath())) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.setPermissions(ownerOnly);
    }
    settings.sync();
    if (QFile::exists(path))
        QFile::setPermissions(path, ownerOnly);
#else
    Q_UNUSED(settings);
#endif
}

} // namespace core
