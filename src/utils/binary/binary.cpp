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

#include "binary.h"

#include <string>

namespace utils::binary {

static inline std::string to_binary_padded_generic(uint64_t value, int width) {
    std::string out;
    out.resize(static_cast<size_t>(width));
    for (int i = 0; i < width; ++i) {
        int bitIndex = width - 1 - i; // MSB first
        out[static_cast<size_t>(i)] = ((value >> bitIndex) & 0x1ULL) ? '1' : '0';
    }
    return out;
}

std::string to_binary_string_padded_8(uint8_t v) {
    return to_binary_padded_generic(static_cast<uint64_t>(v), 8);
}

std::string to_binary_string_padded_16(uint16_t v) {
    return to_binary_padded_generic(static_cast<uint64_t>(v), 16);
}

std::string to_binary_string_padded_32(uint32_t v) {
    return to_binary_padded_generic(static_cast<uint64_t>(v), 32);
}

std::string to_binary_string_padded_64(uint64_t v) {
    return to_binary_padded_generic(v, 64);
}

} // namespace utils::binary
