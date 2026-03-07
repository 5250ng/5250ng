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

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "order_code.h"

namespace tn5250::message::command::order {

/**
 * Represents a TN5250 order.
 */
struct OrderBase {
    OrderCode code;

    /**
     * Unmarshal the order from a byte buffer.
     *
     * @param buffer Input bytes; must contain at least the order code and attributes.
     * @param error  Optional error string; set on failure.
     * @return bytes read on success; 0 on failure.
     */
    virtual uint32_t unmarshal(const std::vector<uint8_t> &buffer, std::string *error = nullptr) = 0;

    /**
     * Marshal the order to a byte buffer.
     *
     * @param error Optional error string; unused for now (reserved for future validation).
     * @return A vector containing the encoded order.
     */
    virtual std::vector<uint8_t> marshal(std::string *error = nullptr) const = 0;

    /**
     * Human-readable description of this order.
     *
     * @param out    Output stream to write to.
     * @param indent Indentation level for pretty-printing.
     */
    virtual void describe(std::ostream &out, int indent) const = 0;
};

} // namespace tn5250::message::command::order