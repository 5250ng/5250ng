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

#include "command/command.h"
#include "header/header.h"
#include "operation_code.h"
#include "utils/endianness/big_endian.h"
#include "utils/hex/hex.h"
#include <algorithm>
#include <cstdint>
#include <list>
#include <ostream>
#include <string>
#include <vector>

namespace tn5250::message {

/**
 * Parsed representation of a single TN5250 General Data Stream (GDS) record.
 *
 * Layout (RFC1205):
 * - recordLength (2 bytes, BE): Length of record body following these two bytes
 * - recordType   (2 bytes, BE): Expected 0x12A0 (General Data Stream)
 * - fixedReserved(2 bytes, BE): Reserved, typically 0x0000
 * - variableLength (1 byte)    : Length of variable header that follows
 * - snaFlags        (1 byte)   : SNA flags
 * - variableReserved(1 byte)   : Reserved, typically 0x00
 * - opcode          (1 byte)   : Operation code (e.g. 0x03 Put/Get)
 * - payload       (remaining)  : Data payload
 */
struct Message {
    // Fixed header (RFC1205 - General Data Stream)
    header::Header header;

    // Variable header
    uint8_t variableLength = 0;   // Length of variable header
    uint8_t snaFlags = 0;         // SNA flags
    uint8_t variableReserved = 0; // Reserved
    OperationCode opcode;         // Operation code

    // Commands in the message (ordered list)
    std::list<command::Command> commands;

    /**
     * Unmarshal a 5250 message from buffer starting at offset 0.
     *
     * @param buffer Input bytes containing a single GDS record (length-prefixed).
     * @param error  Optional error string on failure.
     * @return number of bytes read on success, 0 otherwise.
     */
    uint32_t unmarshal(const std::vector<uint8_t> &buffer, std::string *error = nullptr);

    /**
     * Serialize the message to bytes (wire format).
     *
     * @param error Optional error string on failure.
     * @return Encoded bytes; empty on failure (with error set if provided).
     */
    std::vector<uint8_t> marshal(std::string *error = nullptr) const;

    /**
     * Write a human-readable description to the provided text stream.
     *
     * @param out    Output stream
     * @param indent Indentation level for pretty-printing
     */
    void describe(std::ostream &out, int indent = 0) const;
};

} // namespace tn5250::message
