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

#include "operation_code.h"
#include <iomanip>
#include <sstream>

namespace tn5250::message {

std::string OperationCode::description() const {
    switch (value) {
    case TN5250_OPCODE_NOP:
        return "NOP";
    case TN5250_OPCODE_INVITE:
        return "Invite";
    case TN5250_OPCODE_OUTPUT_ONLY:
        return "Output Only";
    case TN5250_OPCODE_PUT_GET:
        return "Put/Get";
    case TN5250_OPCODE_SAVE_SCREEN:
        return "Save Screen";
    case TN5250_OPCODE_RESTORE_SCREEN:
        return "Restore Screen";
    case TN5250_OPCODE_READ_IMMEDIATE:
        return "Read Immediate";
    case TN5250_OPCODE_READ_SCREEN:
        return "Read Screen";
    case TN5250_OPCODE_CANCEL_INVITE:
        return "Cancel Invite";
    case TN5250_OPCODE_MESSAGE_LIGHT_ON:
        return "Turn On Message Light";
    case TN5250_OPCODE_MESSAGE_LIGHT_OFF:
        return "Turn Off Message Light";
    default: {
        std::ostringstream ss;
        ss << "Unknown (0x" << std::hex << std::nouppercase << std::setfill('0') << std::setw(2)
           << static_cast<unsigned>(value) << ")";
        return ss.str();
    }
    }
}

} // namespace tn5250::message
