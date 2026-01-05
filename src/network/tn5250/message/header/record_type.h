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
