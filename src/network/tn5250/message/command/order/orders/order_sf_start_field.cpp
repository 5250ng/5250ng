#include "order_sf_start_field.h"

namespace tn5250::message::command::order {

/**
 * Unmarshal the order from a byte buffer.
 *
 * @param buffer Input bytes; must contain at least the order code and attributes.
 * @param error  Optional error string; set on failure.
 * @return bytes read on success; 0 on failure.
 */
uint32_t OrderSfStartField::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    if (buffer.size() < 3) {
        if (error)
            *error = "OrderSfStartField: buffer too short for order code and attributes";
        return 0;
    }

    uint32_t read_bytes = 0;

    if (buffer[read_bytes] != OrderCode::START_FIELD) {
        if (error)
            *error = "OrderSfStartField: invalid order code";
        return 0;
    }
    code = OrderCode(buffer[read_bytes]);
    read_bytes++;

    uint8_t b1 = buffer[read_bytes];

    // Field Format Word 1 is present
    if (b1 & 0b01000000) {
        formatWord1 = buffer[read_bytes];
        read_bytes++;

        formatWord1 = buffer[read_bytes];
        read_bytes++;
    }

    // Consume control words until we find no more
    while (read_bytes < buffer.size() && ((buffer[read_bytes] & 0xe0) != 0x20)) {
        controlWords.push_back(buffer[read_bytes] << 8 | buffer[read_bytes + 1]);
        read_bytes += 2;
    }

    // Attributes are the next byte
    attributes = buffer[read_bytes];
    read_bytes++;

    // Length is the next byte
    length = utils::endianness::le16_read(buffer[read_bytes], buffer[read_bytes + 1]);
    read_bytes += 2;

    //
    for (size_t i = 0; i < length; i++) {
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
std::vector<uint8_t> OrderSfStartField::marshal(std::string *error) const {
    std::vector<uint8_t> buffer;

    buffer.push_back(code.value);

    buffer.push_back(formatWord1);
    buffer.push_back(formatWord2);

    for (const auto &controlWord : controlWords) {
        buffer.push_back(controlWord >> 8);
        buffer.push_back(controlWord & 0xff);
    }

    buffer.push_back(attributes);

    uint8_t realLength = repeatedCharacter.size();
    buffer.push_back(realLength);

    for (const auto &ch : repeatedCharacter) {
        buffer.push_back(static_cast<uint8_t>(ch));
    }

    return buffer;
}

/**
 * Write a human-readable representation of the order to an output stream.
 *
 * Example:
 *   <OrderSfStartField>
 *    │ formatWord1     : 0x01 (1)
 *    │ formatWord2     : 0x02 (2)
 *    │ controlWords    : [0x03, 0x04] (3, 4)
 *    │ attributes      : 0x02 (2)
 *    │ length          : 0x03 (3)
 *    │ repeatedCharacter : ["\u0080"]
 *    └───
 *
 * @param out    Output stream to write to.
 * @param indent Indentation level for pretty-printing.
 */
void OrderSfStartField::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<OrderSfStartField>\n";
    out << indentPrompt << "  │ code     : 0x"
        << utils::hex::to_hex_string_padded_2(OrderCode::START_FIELD)
        << " (" << OrderCode(OrderCode::START_FIELD).description() << ")"
        << "\n";
    out << indentPrompt << "  │ formatWord1     : 0x" << utils::hex::to_hex_string_padded_2(formatWord1) << " (" << utils::binary::to_binary_string_padded_8(formatWord1) << ")" << "\n";
    out << indentPrompt << "  │ formatWord2     : 0x" << utils::hex::to_hex_string_padded_2(formatWord2) << " (" << utils::binary::to_binary_string_padded_8(formatWord2) << ")" << "\n";
    out << indentPrompt << "  │ controlWords    : [";
    for (const auto &controlWord : controlWords) {
        out << "0x" << utils::hex::to_hex_string_padded_2(controlWord) << " (" << utils::binary::to_binary_string_padded_16(controlWord) << "), ";
    }
    out << "]\n";
    out << indentPrompt << "  │ attributes      : 0x" << utils::hex::to_hex_string_padded_2(attributes) << " (" << utils::binary::to_binary_string_padded_8(attributes) << ")" << "\n";
    out << indentPrompt << "  │ length          : 0x" << utils::hex::to_hex_string_padded_2(length) << " (" << static_cast<int>(length) << ")" << "\n";
    out << indentPrompt << "  │ repeatedCharacter : [" << repeatedCharacter << "]" << "\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message::command::order