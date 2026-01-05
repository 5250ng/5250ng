#include "message.h"

namespace tn5250::message {

/**
 * Unmarshal a 5250 message from buffer starting at offset 0.
 *
 * @param buffer Input bytes containing a single GDS record (length-prefixed).
 * @param out    Output message.
 * @param error  Optional error string on failure.
 * @return true on success, false otherwise.
 */
uint32_t Message::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    // Unmarshal the fixed header
    uint32_t read_bytes = header.unmarshal(buffer, error);
    if (read_bytes == 0) {
        return 0;
    }

    // Unmarshal the variable header
    variableLength = static_cast<uint8_t>(buffer[read_bytes]);
    read_bytes++;

    // Verify we have enough bytes for variable header
    const int needed = read_bytes + static_cast<int>(variableLength);
    if (buffer.size() < needed) {
        if (error)
            *error = "5250Message: buffer too short for variable header";
        return 0;
    }

    // Variable header bytes (minimum we rely on: flags (1), reserved (1), opcode (1))
    if (variableLength >= 1) {
        snaFlags = static_cast<uint8_t>(buffer[read_bytes]);
        read_bytes++;
    }
    if (variableLength >= 2) {
        variableReserved = static_cast<uint8_t>(buffer[read_bytes]);
        read_bytes++;
    }
    if (variableLength >= 3) {
        opcode = OperationCode(static_cast<uint8_t>(buffer[read_bytes]));
        read_bytes++;
    }

    // Unmarshal the commands
    while (read_bytes < buffer.size()) {
        const std::vector<uint8_t> sub(buffer.begin() + read_bytes, buffer.end());

        if (sub.empty()) {
            if (error) {
                *error = "5250Message: unexpected end of buffer while parsing commands";
            }
            return 0;
        }
        command::Command c;
        uint32_t bytesRead = command::unmarshalCommand(sub, c, error);
        read_bytes += bytesRead;
        if (bytesRead == 0) {
            if (error) {
                *error = "5250Message: error unmarshalling commands -> " + *error;
            }
            return 0;
        }
        commands.push_back(c);
    }

    return read_bytes;
}

std::vector<uint8_t> Message::marshal(std::string *error) const {
    // Determine variable header length (must include at least flags, reserved, opcode)
    uint8_t varLen = variableLength;
    if (varLen < 3) {
        varLen = 3;
    }

    // Compute total record size excluding the 2-byte recordLength field.
    // Fixed fields after length: type(2) + reserved(2) + varLenByte(1) + varLen + payload
    const size_t sizeWithoutLen = 2 + 2 + 1 + static_cast<size_t>(varLen) + commands.size() * 3;
    if (sizeWithoutLen > 0xFFFF) {
        if (error)
            *error = "5250Message: record too large to encode";
        return {};
    }

    std::vector<uint8_t> out;
    out.reserve(2 + sizeWithoutLen);

    // recordLength (big-endian): everything after these two bytes
    utils::endianness::be16_write(out, static_cast<uint16_t>(sizeWithoutLen));

    // recordType (big-endian)
    utils::endianness::be16_write(out, header.recordType.value);
    // fixedReserved (big-endian)
    utils::endianness::be16_write(out, header.fixedReserved);
    // variableLength
    out.push_back(varLen);

    // Variable header content (at least 3 bytes)
    out.push_back(snaFlags);
    out.push_back(variableReserved);
    out.push_back(opcode.value);
    // If caller specified a larger variableLength, pad remaining bytes with zero
    for (uint8_t i = 3; i < varLen; ++i) {
        out.push_back(0x00);
    }

    // Payload
    // out.insert(out.end(), payload.begin(), payload.end());

    return out;
}

/**
 * Write a human-readable description to the provided text stream.
 *
 * @param out    Output stream
 * @param indent Indentation level for pretty-printing
 */
void Message::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<5250Message>\n";
    header.describe(out, indent + 1);
    out << indentPrompt << "  │ variableLength : " << static_cast<unsigned>(variableLength) << "\n";
    out << indentPrompt << "  │ snaFlags       : 0x" << utils::hex::to_hex_string_padded_2(snaFlags) << " (" << utils::binary::to_binary_string_padded_8(snaFlags) << ")" << "\n";
    out << indentPrompt << "  │ Reserved       : 0x" << utils::hex::to_hex_string_padded_2(variableReserved) << " (" << utils::binary::to_binary_string_padded_8(variableReserved) << ")" << "\n";
    out << indentPrompt << "  │ Operation Code : 0x" << utils::hex::to_hex_string_padded_2(static_cast<uint8_t>(opcode.value))
        << " (" << opcode.description() << ")\n";
    out << indentPrompt << "  │ commands (" << commands.size() << "): [\n";
    for (const auto &command : commands) {
        std::visit([&](const auto &cmd) { cmd.describe(out, indent + 1); }, command);
    }
    out << indentPrompt << "  │ ]\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message
