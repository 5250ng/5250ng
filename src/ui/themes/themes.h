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

#include <QApplication>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPalette>
#include <QString>

namespace ui {
namespace themes {

struct Theme {
    QString name;        // unique id, e.g., "dark", "light"
    QString displayName; // user-facing, e.g., "Dark"
    QPalette palette;    // full app palette
    QString stylesheet;  // optional global stylesheet
    QMap<QString, QString> colors; // arbitrary key -> color string ("#rrggbb" or named)
};

} // namespace themes
} // namespace ui
