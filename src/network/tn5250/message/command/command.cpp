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

#include "command.h"

namespace tn5250::message::command {

uint32_t unmarshalCommand(const std::vector<uint8_t> &buffer, Command &out, std::string *error) {
    if (buffer.size() < 2) {
        if (error) {
            *error = "5250Command: buffer too short for ESC and command code";
        }
        return 0;
    }

    // Verify the escape byte is present
    if (buffer[0] != 0x04) {
        if (error) {
            *error = "5250Command: escape byte is not 0x04";
        }
        return 0;
    }

    const uint8_t cmd = buffer[1];
    switch (cmd) {

    case CommandCode::TN5250_COMMAND_CLEAR_UNIT: {
        CommandCsClearScreen cs;
        uint32_t read = cs.unmarshal(buffer, error);
        if (read == 0)
            return 0;
        out = cs;
        return read;
    }

    case CommandCode::TN5250_COMMAND_WRITE_TO_DISPLAY: {
        CommandWtdWriteToDisplay wtd;
        uint32_t read = wtd.unmarshal(buffer, error);
        if (read == 0)
            return 0;
        out = wtd;
        return read;
    }

    case CommandCode::TN5250_COMMAND_READ_MDT_FIELDS: {
        CommandRmfReadMdtFields rmf;
        uint32_t read = rmf.unmarshal(buffer, error);
        if (read == 0)
            return 0;
        out = rmf;
        return read;
    }

    default:
        // Commands without full structured parsers: consume remaining bytes
        // so the caller can continue parsing subsequent commands.
        // This covers: 0x20 (Clear Unit Alternate), 0x21 (Write Error Code),
        // 0x23 (Roll), 0x42 (Read Input Fields), 0x50 (Clear Format Table),
        // 0x72 (Read Immediate), and any future commands.
        return static_cast<uint32_t>(buffer.size());
    }
}

} // namespace tn5250::message::command