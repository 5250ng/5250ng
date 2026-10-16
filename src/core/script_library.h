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

#include <QFileInfoList>
#include <QString>

namespace core {

// Folder holding the user's .5250script library (created on first use).
QString scriptsDirectory();

// Scripts in the library folder, sorted by file name.
QFileInfoList listLibraryScripts();

// Create a new script from the starter template in the library folder,
// picking a free file name derived from `name`. Returns the new file's path,
// or an empty string if it could not be written.
QString createScriptFromTemplate(const QString &name);

} // namespace core
