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

#include "options.h"

#include <QString>
#include <cstdint>

namespace telnet {

QString telnetOptionToString(telnet::TelnetOption opt) {
    using telnet::TelnetOption;
    switch (opt) {
    case TelnetOption::TRANSMIT_BINARY:
        return "BINARY";
    case TelnetOption::ECHO:
        return "ECHO";
    case TelnetOption::SUPPRESS_GO_AHEAD:
        return "SUPPRESS_GO_AHEAD";
    case TelnetOption::TERMINAL_TYPE:
        return "TERMINAL_TYPE";
    case TelnetOption::END_OF_RECORD:
        return "EOR";
    case TelnetOption::NEGOTIATE_ABOUT_WINDOW_SIZE:
        return "NAWS";
    case TelnetOption::TERMINAL_SPEED:
        return "TERMINAL_SPEED";
    case TelnetOption::NEW_ENVIRON:
        return "NEW_ENVIRON";
    case TelnetOption::TN3270E:
        return "TN3270E";
    case TelnetOption::TELNET_START_TLS:
        return "START_TLS";
    default:
        return QString("UNKNOWN(0x%1)")
            .arg(static_cast<uint8_t>(opt), 2, 16, QChar('0'))
            .toUpper();
    }
}

} // namespace telnet