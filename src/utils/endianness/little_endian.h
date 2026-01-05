#pragma once

#include <cstdint>
#include <vector>

namespace utils::endianness {

uint16_t le16_read(uint8_t a1, uint8_t a2);
void le16_write(std::vector<uint8_t> &out, uint16_t v);
uint32_t le32_read(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4);
void le32_write(std::vector<uint8_t> &out, uint32_t v);

} // namespace utils::endianness