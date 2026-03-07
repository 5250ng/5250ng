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

#include "../order_base.h"
#include "utils/binary/binary.h"
#include "utils/hex/hex.h"
#include <iostream>
#include <string>
#include <vector>

namespace tn5250::message::command::order {

/**
 * Repeat to Address (RA) Order
 *
 * @see IBM SA21-9247-6 - IBM 5250 Information Display System Functions Reference Manual
 *
 * Format:
 *
 * RA Order   Bytes 1 and 2           Bytes 3 to ?
 *
 *       X'02'    Row Address   Column Address   Repeated Character
 *
 * This order displays a character in every position starting from the current display address and
 * going to the last position specified by this order. If these two addresses match, 1 character is
 * displayed.
 *
 * Restrictions:
 * A parameter error is posted when:
 * - there are fewer than 3 bytes after the order;
 * - there is a row address value either equal to 0 or greater than 12 for Model 2 (960 characters) or 24 for Model 12 (1920 characters); or
 * - there is a column address value greater than 80 or equal to O.
 * The order is also rejected if the specified ending address is less than the current display address.
 *
 * Note: Although any character can be repeated, avoid
 * using hex 11 (SBA), because this value is used as the delimiter between the fields sent in response to the Read MDT command.
 *
 * Results: The character is repeated from the current
 * display address through the ending display address specified. The current display address is then
 * updated to the value of the last position + 1.
 *
 */
struct OrderRaRepeatToAddress : OrderBase {
    uint8_t rowAddress;
    uint8_t columnAddress;
    std::string repeatedCharacter;

    /**
     * Unmarshal the order from a byte buffer.
     *
     * @param buffer Input bytes; must contain at least the order code and attributes.
     * @param error  Optional error string; set on failure.
     * @return bytes read on success; 0 on failure.
     */
    uint32_t unmarshal(const std::vector<uint8_t> &buffer, std::string *error = nullptr);

    /**
     * Marshal the order to a byte buffer.
     *
     * @param error Optional error string; unused for now (reserved for future validation).
     * @return A vector containing the encoded order.
     */
    std::vector<uint8_t> marshal(std::string *error) const;

    /**
     * Write a human-readable representation of the order to an output stream.
     *
     * Example:
     *   <OrderRaRepeatToAddress>
     *    │ rowAddress    : 0x01 (1)
     *    │ columnAddress : 0x02 (2)
     *    │ repeatedCharacter : ["\u0080"]
     *    └───
     *
     * @param out    Output stream to write to.
     * @param indent Indentation level for pretty-printing.
     */
    void describe(std::ostream &out, int indent) const;
};
} // namespace tn5250::message::command::order