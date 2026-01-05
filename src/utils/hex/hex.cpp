#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace utils::hex {

static inline std::string to_hex_string_padded_generic(uint64_t v, int width) {
    std::ostringstream ss;
    ss << std::hex << std::nouppercase << std::setfill('0') << std::setw(width) << v;
    return ss.str();
}

/**
 * Convert an 8-bit value to a two-character lowercase hexadecimal string.
 *
 * Example: 0x0A -> "0a"
 *
 * @param v 8-bit value to convert.
 * @return Two-character hex string, zero-padded.
 */
std::string to_hex_string_padded_2(uint8_t v) {
    return to_hex_string_padded_generic(static_cast<uint64_t>(v), 2);
}

/**
 * Convert a 16-bit value to a four-character lowercase hexadecimal string.
 *
 * Example: 0x01AF -> "01af"
 *
 * @param v 16-bit value to convert.
 * @return Four-character hex string, zero-padded.
 */
std::string to_hex_string_padded_4(uint16_t v) {
    return to_hex_string_padded_generic(static_cast<uint64_t>(v), 4);
}

/**
 * Convert a 32-bit value to an eight-character lowercase hexadecimal string.
 *
 * Example: 0xDEADBEEF -> "deadbeef"
 *
 * @param v 32-bit value to convert.
 * @return Eight-character hex string, zero-padded.
 */
std::string to_hex_string_padded_8(uint32_t v) {
    return to_hex_string_padded_generic(static_cast<uint64_t>(v), 8);
}

/**
 * Convert a 64-bit value to a sixteen-character lowercase hexadecimal string.
 *
 * Example: 0x0123456789ABCDEF -> "0123456789abcdef"
 *
 * @param v 64-bit value to convert.
 * @return Sixteen-character hex string, zero-padded.
 */
std::string to_hex_string_padded_16(uint64_t v) {
    return to_hex_string_padded_generic(v, 16);
}

/**
 * Produce a classic hex dump of the provided byte buffer.
 *
 * Output format per line:
 *   "00000 0000:  <16 hex bytes with a space after 8th>  <printable ASCII, '.' otherwise>"
 *
 * - 16 bytes per line
 * - After the 8th byte, an extra space is inserted for readability
 * - Non-printable bytes are rendered as '.'
 *
 * @param data Buffer to dump.
 * @return Vector of text lines representing the hex dump.
 */
std::vector<std::string> hexdump(const std::vector<uint8_t> &data) {
    std::vector<std::string> result;
    const int bytesPerLine = 16;

    for (int offset = 0; offset < static_cast<int>(data.size()); offset += bytesPerLine) {
        // Format offset: "00000 0000:"
        int lineOffset = offset;
        std::ostringstream offsetSs;
        offsetSs << std::setfill('0') << std::dec << std::setw(5) << (lineOffset / 65536)
                 << ' ' << std::uppercase << std::hex << std::setw(4) << (lineOffset % 65536) << ':';
        std::string offsetStr = offsetSs.str();

        // Hex bytes: 8 bytes, space, 8 bytes
        std::ostringstream hexSs;
        std::string asciiStr;

        for (int i = 0; i < bytesPerLine; i++) {
            int pos = offset + i;

            if (pos < static_cast<int>(data.size())) {
                uint8_t byte = data[static_cast<size_t>(pos)];
                hexSs << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<unsigned>(byte) << ' ';

                // ASCII representation: printable chars, dots for non-printable
                if (byte >= 0x20 && byte <= 0x7E) {
                    asciiStr.push_back(static_cast<char>(byte));
                } else {
                    asciiStr.push_back('.');
                }
            } else {
                hexSs << "   "; // 3 spaces for missing bytes
                asciiStr.push_back(' ');
            }

            // Add space after 8 bytes
            if (i == 7) {
                hexSs << ' ';
            }
        }

        std::ostringstream line;
        line << offsetStr << "  " << hexSs.str() << "  " << asciiStr;
        result.push_back(line.str());
    }

    return result;
}

} // namespace utils::hex