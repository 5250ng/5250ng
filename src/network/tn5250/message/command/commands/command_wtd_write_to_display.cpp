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

#include "command_wtd_write_to_display.h"

namespace tn5250::message::command {

/**
 * Unmarshal the command from a byte buffer.
 *
 * @param buffer Input bytes; must contain at least the ESC, command code, and control bytes.
 * @param error  Optional error string; set on failure.
 * @return true on success; false on invalid length or malformed input.
 */
uint32_t CommandWtdWriteToDisplay::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    if (buffer.size() < 4) {
        if (error)
            *error = "5250Command: buffer too short for ESC, command code, and control bytes";
        return 0;
    }

    uint32_t read_bytes = 0;
    orders.clear();

    // Verify the escape byte is present
    uint8_t escape_byte = buffer[read_bytes];
    if (escape_byte != 0x04) {
        if (error) {
            *error = "5250Command: escape byte is not 0x04";
        }
        return 0;
    }
    read_bytes++;

    // Verify the command code is correct
    code = CommandCode(buffer[read_bytes]);
    if (code.value != CommandCode::TN5250_COMMAND_WRITE_TO_DISPLAY) {
        if (error) {
            *error = "5250Command: command code is not TN5250_COMMAND_WRITE_TO_DISPLAY (0x11)";
        }
        return 0;
    }
    read_bytes++;

    // Verify the control bytes are present
    control1 = buffer[read_bytes];
    read_bytes++;

    control2 = buffer[read_bytes];
    read_bytes++;

    // Walk orders starting after the control bytes: parse all remaining orders at once
    if (read_bytes < buffer.size()) {
        const std::vector<uint8_t> sub(buffer.begin() + read_bytes, buffer.end());
        if (sub.empty()) {
            if (error) {
                *error = "unexpected end of buffer while parsing orders";
            }
            return 0;
        }
        std::cout << "unmarshalling order(s) at offset " << read_bytes << std::endl;
        for (const auto &line : utils::hex::hexdump(sub)) {
            std::cout << line << std::endl;
            break;
        }
        std::vector<order::Order> parsedOrders = order::unmarshalOrders(sub, error);
        if (parsedOrders.empty() && !sub.empty()) {
            // Failure parsing orders
            return 0;
        }
        for (const auto &o : parsedOrders) {
            orders.push_back(o);
        }
        // We consumed the remainder of the buffer for orders
        read_bytes += static_cast<uint32_t>(sub.size());
    }

    return read_bytes;
};

/**
 * Marshal the command to a byte buffer.
 *
 * @param error Optional error string; unused for now (reserved for future validation).
 * @return A vector containing the encoded command.
 */
std::vector<uint8_t> CommandWtdWriteToDisplay::marshal(std::string *error) const {
    std::vector<uint8_t> buffer;

    buffer.push_back(0x04);
    buffer.push_back(CommandCode::TN5250_COMMAND_WRITE_TO_DISPLAY);
    buffer.push_back(control1);
    buffer.push_back(control2);

    for (const auto &orderVariant : orders) {
        uint8_t orderCodeValue = std::visit([](const auto &ord) { return ord.code.value; }, orderVariant);
        std::vector<uint8_t> marshalledData = std::visit([&](const auto &ord) { return ord.marshal(error); }, orderVariant);
        if (error && !error->empty()) {
            *error = "Command: error marshalling order: " + std::to_string(orderCodeValue);
            return {};
        }
        buffer.insert(buffer.end(), marshalledData.begin(), marshalledData.end());
    }
    return buffer;
};

/**
 * Write a human-readable representation of the order to an output stream.
 *
 * Example:
 *   <Command>
 *    │ code : 0x11 (Write To Display)
 *    │ orders : [
 *    │   <OrderIcInsertCursor>
 *    │    │ rowAddress    : 0x01 (1)
 *    │    │ columnAddress : 0x02 (2)
 *    │    └───
 *    │   <OrderRaRepeatToAddress>
 *    │    │ rowAddress    : 0x03 (3)
 *    │    │ columnAddress : 0x04 (4)
 *    │    │ repeatedCharacter : ["\u0080"]
 *    │    └───
 *    │   <OrderSbaSetBufferAddress>
 *    │    │ rowAddress    : 0x05 (5)
 *    │    │ columnAddress : 0x06 (6)
 *    │    │ repeatedCharacter : ["\u0080"]
 *    │    └───
 *    │ ]
 *    └───
 *
 * @param out    Output stream to write to.
 * @param indent Indentation level for pretty-printing.
 */
void CommandWtdWriteToDisplay::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<CommandWtdWriteToDisplay>\n";
    out << indentPrompt << "  │ code     : 0x"
        << utils::hex::to_hex_string_padded_2(CommandCode::TN5250_COMMAND_WRITE_TO_DISPLAY)
        << " (" << CommandCode(CommandCode::TN5250_COMMAND_WRITE_TO_DISPLAY).description() << ")"
        << "\n";
    out << indentPrompt << "  │ control1 : 0x" << utils::hex::to_hex_string_padded_2(control1) << " (" << utils::binary::to_binary_string_padded_8(control1) << ")" << "\n";
    out << indentPrompt << "  │ control2 : 0x" << utils::hex::to_hex_string_padded_2(control2) << " (" << utils::binary::to_binary_string_padded_8(control2) << ")" << "\n";
    out << indentPrompt << "  │ orders (" << orders.size() << "): [\n";
    for (const auto &order : orders) {
        std::visit([&](const auto &ord) { ord.describe(out, indent + 1); }, order);
    }
    out << indentPrompt << "  │ ]\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message::command