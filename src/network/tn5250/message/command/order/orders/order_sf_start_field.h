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
 * Start Field (SF) Order
 *
 * @see IBM SA21-9247-6 - IBM 5250 Information Display System Functions Reference Manual
 *
 * Function: This order defines input and output fields.
 * If an input field is being defined, it also resets any
 * pending aid byte and locks the keyboard.
 *
 * Note: Although this order can be used for output
 * fields, it is not recommended because it degrades
 * performance. Use the SBA order instead.
 *
 * Restrictions: A parameter error is posted when:
 *  - The output data stream ends before the given number of bytes have been sent.
 *  - The field length is equal to 0, if the field is not signed numeric.
 *    If the field is signed numeric, either a 0 or a 1 causes an error.
 *    Note: The length byte is ignored when an entry is modified in the format table.
 *  - The address for the end of the field exceeds the end of the display.
 *  - The input field addresses are not in ascending order. For input fields defined
 *    by previous Write to Display commands, the input field address must be equal to
 *    the starting address of an already specified field or greater than the last
 *    field already defined.
 *  - Too many input fields are defined for the display.
 *  - Invalid screen attribute is specified.
 *  - The defined input field overlays a previously defined field.
 *

 */
struct OrderSfStartField : OrderBase {
    // 2-Byte field format words (optional)
    uint8_t formatWord1;
    uint8_t formatWord2;

    // 2-Byte field control words (optional) 1 or more
    std::vector<uint16_t> controlWords;

    // Note: See the index entry screen attributes. Bits 0-2
    // must be in the format B'001xxxxx' or an invalid
    // screen attribute error is posted. All other bits are
    // described in the screen attributes text.
    uint8_t attributes;

    uint16_t length;

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
    std::vector<uint8_t> marshal(std::string *error = nullptr) const;

    /**
     * Write a human-readable representation of the order to an output stream.
     *
     * Example:
     *   <OrderSfStartField>
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