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
