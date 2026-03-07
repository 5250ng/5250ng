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

#include "order_soh_start_of_header.h"

namespace tn5250::message::command::order {

/**
 * Unmarshal the order from a byte buffer.
 *
 * @param buffer Input bytes; must contain at least the order code and attributes.
 * @param error  Optional error string; set on failure.
 * @return bytes read on success; 0 on failure.
 */
uint32_t OrderSohStartOfHeader::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    if (buffer.size() < 3) {
        if (error)
            *error = "OrderSohStartOfHeader: buffer too short for order code and attributes";
        return 0;
    }

    uint32_t read_bytes = 0;

    if (buffer[read_bytes] != OrderCode::START_OF_HEADER) {
        if (error)
            *error = "OrderSohStartOfHeader: invalid order code";
        return 0;
    }
    code = OrderCode(buffer[read_bytes]);
    read_bytes++;

    length = buffer[read_bytes];
    read_bytes++;

    flags = buffer[read_bytes];
    read_bytes++;

    flagsReserved = buffer[read_bytes];
    read_bytes++;

    resequenceToField = buffer[4];
    read_bytes++;

    errorRow = buffer[read_bytes];
    read_bytes++;

    commandKeySwitch1 = buffer[read_bytes];
    read_bytes++;

    commandKeySwitch2 = buffer[read_bytes];
    read_bytes++;

    commandKeySwitch3 = buffer[read_bytes];
    read_bytes++;

    return read_bytes;
}

/**
 * Marshal the order to a byte buffer.
 *
 * @param error Optional error string; unused for now (reserved for future validation).
 * @return A vector containing the encoded order.
 */
std::vector<uint8_t> OrderSohStartOfHeader::marshal(std::string *error) const {
    std::vector<uint8_t> buffer;
    buffer.push_back(code.value);
    buffer.push_back(length);
    buffer.push_back(flags);
    buffer.push_back(flagsReserved);
    buffer.push_back(resequenceToField);
    buffer.push_back(errorRow);
    buffer.push_back(commandKeySwitch1);
    buffer.push_back(commandKeySwitch2);
    buffer.push_back(commandKeySwitch3);
    return buffer;
}

/**
 * Write a human-readable representation of the order to an output stream.
 *
 * Example:
 *   <OrderSohStartOfHeader>
 *    │ length        : 0x01 (1)
 *    │ flags         : 0x02 (2)
 *    │ flagsReserved : 0x03 (3)
 *    │ resequenceToField : 0x04 (4)
 *    │ errorRow      : 0x05 (5)
 *    │ commandKeySwitch1 : 0x06 (6)
 *    │ commandKeySwitch2 : 0x07 (7)
 *    │ commandKeySwitch3 : 0x08 (8)
 *    └───
 *
 * @param out    Output stream to write to.
 * @param indent Indentation level for pretty-printing.
 */
void OrderSohStartOfHeader::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<OrderSohStartOfHeader>\n";
    out << indentPrompt << "  │ code     : 0x"
        << utils::hex::to_hex_string_padded_2(OrderCode::START_OF_HEADER)
        << " (" << OrderCode(OrderCode::START_OF_HEADER).description() << ")"
        << "\n";
    out << indentPrompt << "  │ length            : 0x" << utils::hex::to_hex_string_padded_2(length) << " (" << length << ")" << "\n";
    out << indentPrompt << "  │ flags             : 0x" << utils::hex::to_hex_string_padded_2(flags) << " (" << utils::binary::to_binary_string_padded_8(flags) << ")" << "\n";
    out << indentPrompt << "  │ flagsReserved     : 0x" << utils::hex::to_hex_string_padded_2(flagsReserved) << " (" << utils::binary::to_binary_string_padded_8(flagsReserved) << ")" << "\n";
    out << indentPrompt << "  │ resequenceToField : 0x" << utils::hex::to_hex_string_padded_2(resequenceToField) << " (" << utils::binary::to_binary_string_padded_8(resequenceToField) << ")" << "\n";
    out << indentPrompt << "  │ errorRow          : 0x" << utils::hex::to_hex_string_padded_2(errorRow) << " (" << static_cast<int>(errorRow) << ")" << "\n";
    out << indentPrompt << "  │ commandKeySwitch1 : 0x" << utils::hex::to_hex_string_padded_2(commandKeySwitch1) << " (" << utils::binary::to_binary_string_padded_8(commandKeySwitch1) << ")" << "\n";
    out << indentPrompt << "  │ commandKeySwitch2 : 0x" << utils::hex::to_hex_string_padded_2(commandKeySwitch2) << " (" << utils::binary::to_binary_string_padded_8(commandKeySwitch2) << ")" << "\n";
    out << indentPrompt << "  │ commandKeySwitch3 : 0x" << utils::hex::to_hex_string_padded_2(commandKeySwitch3) << " (" << utils::binary::to_binary_string_padded_8(commandKeySwitch3) << ")" << "\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message::command::order