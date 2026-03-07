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

#include <cstdint>
#include <string>

namespace tn5250::message::command {

/**
 * Represents a TN5250 command code.
 */
struct CommandCode {
    uint8_t value;

    // Known TN5250 commands (unscoped enum with fixed underlying type)
    enum : uint8_t {
        TN5250_COMMAND_ERASE_WRITE = 0x05,
        TN5250_COMMAND_READ_MODIFY = 0x06,
        TN5250_COMMAND_READ_MODIFY_WRITE = 0x07,
        TN5250_COMMAND_ERASE_WRITE_ALTERNATE = 0x0D,
        TN5250_COMMAND_WRITE_TO_DISPLAY = 0x11,
        TN5250_COMMAND_CLEAR_UNIT_ALTERNATE = 0x20,
        TN5250_COMMAND_WRITE_ERROR_CODE = 0x21,
        TN5250_COMMAND_ROLL = 0x23,
        TN5250_COMMAND_CLEAR_UNIT = 0x40,
        TN5250_COMMAND_READ_INPUT_FIELDS = 0x42,
        TN5250_COMMAND_CLEAR_FORMAT_TABLE = 0x50,
        TN5250_COMMAND_READ_MDT_FIELDS = 0x52,
        TN5250_COMMAND_READ_IMMEDIATE = 0x72,
    };

    constexpr CommandCode() : value(0) {}
    constexpr explicit CommandCode(uint16_t v) : value(v) {}

    /**
     * Human-readable description of this command code.
     *
     * @return A short string describing the command code; includes hex code for unknown command codes.
     */
    std::string description() const;
};

} // namespace tn5250::message::command
