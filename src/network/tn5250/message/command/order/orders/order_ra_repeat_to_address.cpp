#include "order_ra_repeat_to_address.h"

namespace tn5250::message::command::order {

/**
 * Unmarshal the order from a byte buffer.
 *
 * @param buffer Input bytes; must contain at least the order code and attributes.
 * @param error  Optional error string; set on failure.
 * @return bytes read on success; 0 on failure.
 */
uint32_t OrderRaRepeatToAddress::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    if (buffer.size() < 3) {
        if (error)
            *error = "OrderRaRepeatToAddress: buffer too short for order code and attributes";
        return 0;
    }

    if (buffer[0] != OrderCode::REPEAT_TO_ADDRESS) {
        if (error)
            *error = "OrderRaRepeatToAddress: invalid order code";
        return 0;
    }

    code = OrderCode(OrderCode::REPEAT_TO_ADDRESS);
    rowAddress = buffer[1];
    columnAddress = buffer[2];
    repeatedCharacter = std::string(1, buffer[3]);

    return 4;
}

/**
 * Marshal the order to a byte buffer.
 *
 * @param error Optional error string; unused for now (reserved for future validation).
 * @return A vector containing the encoded order.
 */
std::vector<uint8_t> OrderRaRepeatToAddress::marshal(std::string *error) const {
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
 *   <OrderRaRepeatToAddress>
 *    │ rowAddress    : 0x01 (1)
 *    │ columnAddress : 0x02 (2)
 *    │ repeatedCharacter : ["\u0080"]
 *    └───
 *
 * @param out    Output stream to write to.
 * @param indent Indentation level for pretty-printing.
 */
void OrderRaRepeatToAddress::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<OrderRaRepeatToAddress>\n";
    out << indentPrompt << "  │ code     : 0x"
        << utils::hex::to_hex_string_padded_2(OrderCode::REPEAT_TO_ADDRESS)
        << " (" << OrderCode(OrderCode::REPEAT_TO_ADDRESS).description() << ")"
        << "\n";
    out << indentPrompt << "  │ rowAddress    : 0x" << utils::hex::to_hex_string_padded_2(rowAddress) << " (" << static_cast<int>(rowAddress) << ")" << "\n";
    out << indentPrompt << "  │ columnAddress : 0x" << utils::hex::to_hex_string_padded_2(columnAddress) << " (" << static_cast<int>(columnAddress) << ")" << "\n";
    out << indentPrompt << "  │ repeatedCharacter : [" << repeatedCharacter << "]" << "\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message::command::order