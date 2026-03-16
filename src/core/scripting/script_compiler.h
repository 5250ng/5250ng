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

#include "core/macro_recorder.h"
#include <QMap>
#include <QRegularExpression>
#include <QString>

namespace core::scripting {

struct ScriptMetadata {
    QMap<QString, QString> values;
    QString name() const        { return values.value("script.name"); }
    QString author() const      { return values.value("script.author"); }
    QString description() const { return values.value("script.description"); }
    QString version() const     { return values.value("script.version"); }
    QString menuPath() const    { return values.value("menu.path"); }
};

class ScriptCompiler {
  public:
    static QString macroToScript(const Macro &macro);
    static QString aidByteToKeyword(uint8_t aidByte);
    static ScriptMetadata extractMetadata(const QString &scriptText);
};

} // namespace core::scripting
