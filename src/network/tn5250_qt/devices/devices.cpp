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

#include "devices.h"

namespace tn5250::devices {

const QVector<Device> &supportedDevices() {
    static const QVector<Device> kDevices = {
        // IBM-5555-C01   24 x 80 Double-Byte Character Set color display
        {"IBM-5555-C01", 24, 80, true, true},
        // IBM-5555-B01   24 x 80 Double-Byte Character Set (DBCS)
        {"IBM-5555-B01", 24, 80, false, true},
        // IBM-3477-FC    27 x 132 color display
        {"IBM-3477-FC", 27, 132, true, false},
        // IBM-3477-FG    27 x 132 monochrome display
        {"IBM-3477-FG", 27, 132, false, false},
        // IBM-3180-2     27 x 132 monochrome display
        {"IBM-3180-2", 27, 132, false, false},
        // IBM-3179-2     24 x 80 color display
        {"IBM-3179-2", 24, 80, true, false},
        // IBM-3196-A1    24 x 80 monochrome display
        {"IBM-3196-A1", 24, 80, false, false},
        // IBM-5292-2     24 x 80 color display
        {"IBM-5292-2", 24, 80, true, false},
        // IBM-5291-1     24 x 80 monochrome display
        {"IBM-5291-1", 24, 80, false, false},
        // IBM-5251-11    24 x 80 monochrome display
        {"IBM-5251-11", 24, 80, false, false},
    };
    return kDevices;
}

const Device *findSupportedDevice(const QString &model) {
    const auto &list = supportedDevices();
    for (const auto &device : list) {
        if (device.model.compare(model, Qt::CaseInsensitive) == 0) {
            return &device;
        }
    }
    return nullptr;
}

} // namespace tn5250::devices
