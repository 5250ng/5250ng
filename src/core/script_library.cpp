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
#include "script_library.h"
#include "core/macro_recorder.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace core {

QString scriptsDirectory() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + "/scripts";
    QDir().mkpath(dir);
    return dir;
}

QFileInfoList listLibraryScripts() {
    return QDir(scriptsDirectory()).entryInfoList({"*.5250script"}, QDir::Files, QDir::Name);
}

QString createScriptFromTemplate(const QString &name) {
    QString safeName = MacroRecorder::sanitizeFileName(name);
    QDir dir(scriptsDirectory());
    QString path = dir.filePath(safeName + ".5250script");
    int suffix = 1;
    while (QFile::exists(path))
        path = dir.filePath(safeName + "_" + QString::number(suffix++) + ".5250script");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QString();
    file.write(QString(
        "# @script.name = \"%1\"\n"
        "# @script.description = \"\"\n"
        "# @script.version = \"1.0\"\n"
        "# @menu.path = \"%1\"\n"
        "\n"
        "EXPECT KEYBOARD UNLOCKED\n"
        "TYPE \"hello\"\n"
        "PRESS ENTER\n"
    ).arg(name).toUtf8());
    file.close();
    return path;
}

} // namespace core
