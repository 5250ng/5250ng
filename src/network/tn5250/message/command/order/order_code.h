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

namespace tn5250::message::command::order {

/**
 * TN5250 order code appearing in Write commands (RFC1205).
 */
struct OrderCode {
    uint8_t value;

    // Known TN5250 display orders (unscoped enum with fixed underlying type)
    // Source: IBM SA21-9247-6 - IBM 5250 Information Display System Functions Reference Manual, page 2-136
    enum : uint8_t {
        START_OF_HEADER = 0x01,
        REPEAT_TO_ADDRESS = 0x02,
        INSERT_CURSOR = 0x03,
        SET_BUFFER_ADDRESS = 0x11,
        START_FIELD = 0x1D
    };

    constexpr OrderCode() : value(0) {}
    constexpr explicit OrderCode(uint8_t v) : value(v) {}

    /**
     * Human-readable description of this order.
     *
     * @return A short string describing the order; includes hex code for unknown orders.
     */
    std::string description() const;
};

} // namespace tn5250::message::command::order
