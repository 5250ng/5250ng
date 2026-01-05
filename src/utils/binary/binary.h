#pragma once

#include <cstdint>
#include <string>

namespace utils::binary {

// Return fixed-width binary strings (zero-padded) for integral types
std::string to_binary_string_padded_8(uint8_t v);
std::string to_binary_string_padded_16(uint16_t v);
std::string to_binary_string_padded_32(uint32_t v);
std::string to_binary_string_padded_64(uint64_t v);

} // namespace utils::binary
