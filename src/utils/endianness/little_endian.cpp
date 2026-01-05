#include <cstdint>
#include <vector>

namespace utils::endianness {

/**
 * Decode a 16-bit unsigned integer from two little-endian bytes.
 *
 * @param lo Low-order byte.
 * @param hi High-order byte.
 * @return The combined 16-bit value (lo | hi<<8).
 */
uint16_t le16_read(uint8_t lo, uint8_t hi) {
    return static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8) | static_cast<uint16_t>(lo));
}

/**
 * Append a 16-bit unsigned integer to a byte buffer in little-endian order.
 *
 * @param out Destination buffer to append to.
 * @param v   Value to encode (least significant byte first).
 */
void le16_write(std::vector<uint8_t> &out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

/**
 * Decode a 32-bit unsigned integer from four little-endian bytes.
 *
 * @param a1 Least significant byte.
 * @param a2 Next significant byte.
 * @param a3 Next significant byte.
 * @param a4 Most significant byte.
 * @return The combined 32-bit value.
 */
uint32_t le32_read(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4) {
    return static_cast<uint32_t>((static_cast<uint32_t>(a4) << 24) | (static_cast<uint32_t>(a3) << 16) | (static_cast<uint32_t>(a2) << 8) | static_cast<uint32_t>(a1));
}

/**
 * Append a 32-bit unsigned integer to a byte buffer in little-endian order.
 *
 * @param out Destination buffer to append to.
 * @param v   Value to encode (least significant byte first).
 */
void le32_write(std::vector<uint8_t> &out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

} // namespace utils::endianness
