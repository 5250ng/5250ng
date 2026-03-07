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

#include "order_sba_set_buffer_address.h"

namespace tn5250::message::command::order {

/**
 * Unmarshal the order from a byte buffer.
 *
 * @param buffer Input bytes; must contain at least the order code and attributes.
 * @param error  Optional error string; set on failure.
 * @return bytes read on success; 0 on failure.
 */
uint32_t OrderSbaSetBufferAddress::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    if (buffer.size() < 4) {
        if (error)
            *error = "OrderSbaSetBufferAddress: buffer too short for order code and attributes";
        return 0;
    }

    uint32_t read_bytes = 0;

    if (buffer[read_bytes] != OrderCode::SET_BUFFER_ADDRESS) {
        if (error)
            *error = "OrderSbaSetBufferAddress: invalid order code";
        return 0;
    }
    code = OrderCode(buffer[read_bytes]);
    read_bytes++;

    rowAddress = buffer[read_bytes];
    read_bytes++;

    columnAddress = buffer[read_bytes];
    read_bytes++;

    // Consume repeated character bytes until the start of a next order code or end of buffer
    auto isOrderCode = [](uint8_t b) -> bool {
        switch (b) {
        case OrderCode::START_OF_HEADER:
        case OrderCode::REPEAT_TO_ADDRESS:
        case OrderCode::INSERT_CURSOR:
        case OrderCode::SET_BUFFER_ADDRESS:
        case OrderCode::START_FIELD:
            return true;
        default:
            return false;
        }
    };
    while (read_bytes < buffer.size() && !isOrderCode(buffer[read_bytes])) {
        repeatedCharacter.push_back(static_cast<char>(buffer[read_bytes]));
        read_bytes++;
    }

    return read_bytes;
}

/**
 * Marshal the order to a byte buffer.
 *
 * @param error Optional error string; unused for now (reserved for future validation).
 * @return A vector containing the encoded order.
 */
std::vector<uint8_t> OrderSbaSetBufferAddress::marshal(std::string *error) const {
    std::vector<uint8_t> buffer;
    buffer.push_back(code.value);
    buffer.push_back(rowAddress);
    buffer.push_back(columnAddress);
    buffer.push_back(repeatedCharacter[0]);
    return buffer;
}

/**
 * Write a human-readable representation of the order to an output stream.
 *
 * Example:
 *   <OrderSbaSetBufferAddress>
 *    │ rowAddress    : 0x01 (1)
 *    │ columnAddress : 0x02 (2)
 *    │ repeatedCharacter : ["\u0080"]
 *    └───
 *
 * @param out    Output stream to write to.
 * @param indent Indentation level for pretty-printing.
 */
void OrderSbaSetBufferAddress::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<OrderSbaSetBufferAddress>\n";
    out << indentPrompt << "  │ code     : 0x"
        << utils::hex::to_hex_string_padded_2(OrderCode::SET_BUFFER_ADDRESS)
        << " (" << OrderCode(OrderCode::SET_BUFFER_ADDRESS).description() << ")"
        << "\n";
    out << indentPrompt << "  │ rowAddress    : 0x" << utils::hex::to_hex_string_padded_2(rowAddress) << " (" << static_cast<int>(rowAddress) << ")" << "\n";
    out << indentPrompt << "  │ columnAddress : 0x" << utils::hex::to_hex_string_padded_2(columnAddress) << " (" << static_cast<int>(columnAddress) << ")" << "\n";
    out << indentPrompt << "  │ repeatedCharacter : [" << repeatedCharacter << "]" << "\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message::command::order