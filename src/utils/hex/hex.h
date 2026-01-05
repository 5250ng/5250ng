#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace utils::hex {

std::string to_hex_string_padded_2(uint8_t v);
std::string to_hex_string_padded_4(uint16_t v);
std::string to_hex_string_padded_8(uint32_t v);
std::string to_hex_string_padded_16(uint64_t v);
std::vector<std::string> hexdump(const std::vector<uint8_t> &data);

} // namespace utils::hex