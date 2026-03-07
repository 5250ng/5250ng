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

#include <cstdint>
#include <vector>

namespace utils::endianness {

/**
 * Decode a 16-bit unsigned integer from two big-endian bytes.
 *
 * @param hi High-order byte.
 * @param lo Low-order byte.
 * @return The combined 16-bit value (hi<<8 | lo).
 */
uint16_t be16_read(uint8_t a1, uint8_t a2) {
    return static_cast<uint16_t>((static_cast<uint16_t>(a1) << 8) | static_cast<uint16_t>(a2));
}

/**
 * Append a 16-bit unsigned integer to a byte buffer in big-endian order.
 *
 * @param out Destination buffer to append to.
 * @param v   Value to encode (most significant byte first).
 */
void be16_write(std::vector<uint8_t> &out, uint16_t v) {
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

/**
 * Decode a 32-bit unsigned integer from four big-endian bytes.
 *
 * @param a1 Most significant byte.
 * @param a2 Next significant byte.
 * @param a3 Next significant byte.
 * @param a4 Least significant byte.
 * @return The combined 32-bit value.
 */
uint32_t be32_read(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4) {
    return static_cast<uint32_t>((static_cast<uint32_t>(a1) << 24) | (static_cast<uint32_t>(a2) << 16) | (static_cast<uint32_t>(a3) << 8) | static_cast<uint32_t>(a4));
}

/**
 * Append a 32-bit unsigned integer to a byte buffer in big-endian order.
 *
 * @param out Destination buffer to append to.
 * @param v   Value to encode (most significant byte first).
 */
void be32_write(std::vector<uint8_t> &out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

} // namespace utils::endianness