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
#include <string>

namespace tn5250::message::header {

/**
 * Represents a SNA record type.
 */
struct RecordType {
    uint16_t value;

    constexpr RecordType() : value(0) {}
    constexpr explicit RecordType(uint16_t v) : value(v) {}

    // Known SNA record types (unscoped enum with fixed underlying type)
    enum : uint16_t {
        SNA_RECORD_TYPE_GENERAL_DATA_STREAM = 0x12A0
    };

    constexpr bool isGeneralDataStream() const { return value == SNA_RECORD_TYPE_GENERAL_DATA_STREAM; }

    /**
     * Human-readable description of this record type.
     *
     * @return A short string describing the record type; includes hex code for unknown types.
     */
    std::string description() const;
};

} // namespace tn5250::message::header
