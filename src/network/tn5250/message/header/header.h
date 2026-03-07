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

#pragma once

#include "record_type.h"
#include "utils/binary/binary.h"
#include "utils/endianness/big_endian.h"
#include "utils/hex/hex.h"
#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace tn5250::message::header {

struct Header {
    // Fixed header (RFC1205 - General Data Stream)
    uint16_t recordLength = 0;  // Big-endian
    RecordType recordType;      // SNA record type
    uint16_t fixedReserved = 0; // Usually 0x0000

    /**
     * Unmarshal the fixed 5250 header from a byte buffer.
     *
     * @param buffer Input bytes; must contain at least 6 bytes starting at offset 0.
     * @param error  Optional error string; set on failure.
     * @return true on success; false on invalid length or malformed input.
     */
    uint32_t unmarshal(const std::vector<uint8_t> &buffer, std::string *error = nullptr);

    /**
     * Marshal the fixed 5250 header fields to a big-endian byte buffer.
     *
     * @param error Optional error string; unused for now (reserved for future validation).
     * @return A vector containing the encoded header (exactly 6 bytes).
     */
    std::vector<uint8_t> marshal(std::string *error = nullptr) const;

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
    void describe(std::ostream &out, int indent = 0) const;
};

} // namespace tn5250::message::header
