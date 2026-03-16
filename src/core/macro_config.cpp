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

#include "macro_config.h"
#include <QSettings>

namespace core {

MacroConfig::MacroConfig() {}

MacroConfig &MacroConfig::instance() {
    static MacroConfig s;
    return s;
}

void MacroConfig::load() {
    QSettings settings;
    settings.beginGroup("Macros");
    m_recordTimings = settings.value("recordTimings", false).toBool();
    settings.endGroup();
}

void MacroConfig::save() {
    QSettings settings;
    settings.beginGroup("Macros");
    settings.setValue("recordTimings", m_recordTimings);
    settings.endGroup();
}

} // namespace core
