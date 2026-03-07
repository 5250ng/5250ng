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

#include "command_code.h"
#include <iomanip>
#include <sstream>

namespace tn5250::message::command {

/**
 * Human-readable description of this command code.
 *
 * @return A short string describing the command code; includes hex code for unknown command codes.
 */
std::string CommandCode::description() const {
    switch (value) {
    case TN5250_COMMAND_WRITE_TO_DISPLAY:
        return "Write To Display";
    case TN5250_COMMAND_ERASE_WRITE:
        return "Erase Write";
    case TN5250_COMMAND_ERASE_WRITE_ALTERNATE:
        return "Erase Write Alternate";
    case TN5250_COMMAND_READ_MODIFY:
        return "Read Modify";
    case TN5250_COMMAND_CLEAR_UNIT:
        return "Clear Unit";
    case TN5250_COMMAND_CLEAR_UNIT_ALTERNATE:
        return "Clear Unit Alternate";
    case TN5250_COMMAND_WRITE_ERROR_CODE:
        return "Write Error Code";
    case TN5250_COMMAND_ROLL:
        return "Roll";
    case TN5250_COMMAND_READ_INPUT_FIELDS:
        return "Read Input Fields";
    case TN5250_COMMAND_CLEAR_FORMAT_TABLE:
        return "Clear Format Table";
    case TN5250_COMMAND_READ_MDT_FIELDS:
        return "Read MDT Fields";
    case TN5250_COMMAND_READ_MODIFY_WRITE:
        return "Read Modify Write";
    case TN5250_COMMAND_READ_IMMEDIATE:
        return "Read Immediate";
    default: {
        std::ostringstream ss;
        ss << "Unknown (0x" << std::hex << std::nouppercase << std::setfill('0') << std::setw(2)
           << value << ")";
        return ss.str();
    }
    }
}

} // namespace tn5250::message::command
