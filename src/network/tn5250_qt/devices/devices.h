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

#include <QString>
#include <QVector>
#include <cstdint>

namespace tn5250::devices {

struct Device {
    QString model;
    int lines;
    int columns;
    bool supportsColors;
    bool supportsDbcs;
};

// Return immutable list of supported devices
const QVector<Device> &supportedDevices();

// Find a device by exact model name; returns nullptr if not found
const Device *findSupportedDevice(const QString &model);

} // namespace tn5250::devices
