#include "order_ic_insert_cursor.h"

namespace tn5250::message::command::order {

/**
 * Unmarshal the order from a byte buffer.
 *
 * @param buffer Input bytes; must contain at least the order code and attributes.
 * @param error  Optional error string; set on failure.
 * @return bytes read on success; 0 on failure.
 */
uint32_t OrderIcInsertCursor::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    if (buffer.size() < 3) {
        if (error)
            *error = "OrderIcInsertCursor: buffer too short for order code and attributes";
        return 0;
    }

    if (buffer[0] != OrderCode::INSERT_CURSOR) {
        if (error)
            *error = "OrderIcInsertCursor: invalid order code";
        return 0;
    }

    code = OrderCode(OrderCode::INSERT_CURSOR);
    rowAddress = buffer[1];
    columnAddress = buffer[2];

    return 3;
}

/**
 * Marshal the order to a byte buffer.
 *
 * @param error Optional error string; unused for now (reserved for future validation).
 * @return A vector containing the encoded order.
 */
std::vector<uint8_t> OrderIcInsertCursor::marshal(std::string *error) const {
    std::vector<uint8_t> buffer;
    buffer.push_back(code.value);
    buffer.push_back(rowAddress);
    buffer.push_back(columnAddress);
    return buffer;
}

/**
 * Write a human-readable representation of the order to an output stream.
 *
 * Example:
 *   <OrderIcInsertCursor>
 *    │ rowAddress    : 0x01 (1)
 *    │ columnAddress : 0x02 (2)
 *    └───
 *
 * @param out    Output stream to write to.
 * @param indent Indentation level for pretty-printing.
 */
void OrderIcInsertCursor::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<OrderIcInsertCursor>\n";
    out << indentPrompt << "  │ code     : 0x"
        << utils::hex::to_hex_string_padded_2(OrderCode::INSERT_CURSOR)
        << " (" << OrderCode(OrderCode::INSERT_CURSOR).description() << ")"
        << "\n";
    out << indentPrompt << "  │ rowAddress    : 0x" << utils::hex::to_hex_string_padded_2(rowAddress) << " (" << rowAddress << ")" << "\n";
    out << indentPrompt << "  │ columnAddress : 0x" << utils::hex::to_hex_string_padded_2(columnAddress) << " (" << columnAddress << ")" << "\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message::command::order