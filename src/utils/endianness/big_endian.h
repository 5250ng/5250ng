#pragma once

#include <cstdint>
#include <vector>

namespace utils::endianness {

uint16_t be16_read(uint8_t a1, uint8_t a2);
void be16_write(std::vector<uint8_t> &out, uint16_t v);
uint32_t be32_read(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4);
void be32_write(std::vector<uint8_t> &out, uint32_t v);

} // namespace utils::endianness