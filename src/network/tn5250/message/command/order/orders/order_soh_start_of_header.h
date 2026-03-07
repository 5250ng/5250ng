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
#include "utils/endianness/little_endian.h"
#include "utils/hex/hex.h"
#include <iostream>
#include <string>
#include <vector>

namespace tn5250::message::command::order {

/**
 * Start Of Header (SOH) Order
 *
 * @see IBM SA21-9247-6 - IBM 5250 Information Display System Functions Reference Manual
 *
 * Function: This order specifies the header information that goes into the format table.
 * See the index entry format table.
 * It also selects the resequencing function when data is read from the display.
 * See the index entry field control word.
 *
 * Restrictions: A parameter error is posted when the output data stream ends before the
 * number of bytes needed have been sent or when the first byte of the order is not between 1 and 254
 *
 * Format:
 * Order      Length      Variable Bytes
 * Hex 01     (Note 1.)   (Note 2.)
 *
 * Note 1:
 * The length byte determines the number of header bytes following the length byte, but this number
 * does not include the length byte. It must be greater than 0 and less than 255.
 *
 * Note 2:
 * This chart shows what the bytes following the length byte can contain:
 * Byte Description
 * 1    Reserved
 * 2    Format ID - from hex 00 to hex FF
 * 3    Hex 00 = no resequencing (all resequencing field control words are ignored).
 *      Hex xx = resequence per field control words in format table. See the index entry field
 *      control word. (For example, xx = 01 = field 1). Byte 3 also points to the first field
 *      to be returned when the Read Input Fields/Read MDT command is serviced.
 * 4    Row address of the operator error line. If this is unspecified or out of range, the error
 *      line defaults to the last line of the display, or to the message line, if supported.
 * 5-7  The following chart shows the data- included switches for the command function keys.
 */
struct OrderSohStartOfHeader : OrderBase {
    uint8_t length;
    uint8_t flags;
    uint8_t flagsReserved;
    uint8_t resequenceToField;
    uint8_t errorRow;
    uint8_t commandKeySwitch1;
    uint8_t commandKeySwitch2;
    uint8_t commandKeySwitch3;

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
    std::vector<uint8_t> marshal(std::string *error = nullptr) const;

    /**
     * Write a human-readable representation of the order to an output stream.
     *
     * Example:
     *   <OrderSohStartOfHeader>
     *    │ rowAddress    : 0x01 (1)
     *    │ columnAddress : 0x02 (2)
     *    └───
     *
     * @param out    Output stream to write to.
     * @param indent Indentation level for pretty-printing.
     */
    void describe(std::ostream &out, int indent) const;
};

} // namespace tn5250::message::command::order