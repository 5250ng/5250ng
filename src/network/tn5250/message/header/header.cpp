#include "header.h"

namespace tn5250::message::header {

/**
 * Unmarshal the fixed 5250 header from a byte buffer.
 *
 * Expected layout (big-endian):
 *   - recordLength   (2 bytes)  Total length of the record body after these two bytes
 *   - recordType     (2 bytes)  SNA record type (e.g., 0x12A0 for General Data Stream)
 *   - fixedReserved  (2 bytes)  Reserved, typically 0x0000
 *
 * @param buffer Input bytes; must contain at least 6 bytes starting at offset 0.
 * @param error  Optional error string; set on failure.
 * @return true on success; false on invalid length or malformed input.
 */
uint32_t Header::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    if (buffer.size() < 6) {
        if (error)
            *error = "5250Header: buffer too short for fixed header";
        return 0;
    }

    recordLength = utils::endianness::be16_read(buffer[0], buffer[1]);
    recordType = RecordType(utils::endianness::be16_read(buffer[2], buffer[3]));
    fixedReserved = utils::endianness::be16_read(buffer[4], buffer[5]);

    return 6;
}

/**
 * Marshal the fixed 5250 header fields to a big-endian byte buffer.
 *
 * Output layout:
 *   - recordLength   (2 bytes, BE)
 *   - recordType     (2 bytes, BE)
 *   - fixedReserved  (2 bytes, BE)
 *
 * @param error Optional error string; unused for now (reserved for future validation).
 * @return A vector containing the encoded header (exactly 6 bytes).
 */
std::vector<uint8_t> Header::marshal(std::string *error) const {
    std::vector<uint8_t> out;
    out.reserve(6);
    utils::endianness::be16_write(out, recordLength);
    utils::endianness::be16_write(out, recordType.value);
    utils::endianness::be16_write(out, fixedReserved);
    return out;
}

/**
 * Write a human-readable representation of the header to an output stream.
 *
 * Example:
 *   <Header>
 *    │ recordLength  : 0x001F (31)
 *    │ recordType    : 0x12a0
 *    │ fixedReserved : 0x0000
 *    └───
 *
 * @param out    Output stream to write to.
 * @param indent Indentation level for pretty-printing.
 */
void Header::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<Header>\n";
    out << indentPrompt << "  │ recordLength  : 0x" << utils::hex::to_hex_string_padded_4(recordLength) << " (" << recordLength << ")" << "\n";
    out << indentPrompt << "  │ recordType    : 0x" << utils::hex::to_hex_string_padded_4(recordType.value) << " (" << recordType.description() << ")" << "\n";
    out << indentPrompt << "  │ fixedReserved : 0x" << utils::hex::to_hex_string_padded_4(fixedReserved) << "(" << utils::binary::to_binary_string_padded_16(fixedReserved) << ")" << "\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message::header