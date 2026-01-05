#pragma once

#include <cstdint>
#include <string>

namespace tn5250::message::command {

/**
 * Represents a TN5250 command code.
 */
struct CommandCode {
    uint8_t value;

    // Known TN5250 commands (unscoped enum with fixed underlying type)
    enum : uint8_t {
        TN5250_COMMAND_ERASE_WRITE = 0x05,
        TN5250_COMMAND_READ_MODIFY = 0x06,
        TN5250_COMMAND_READ_MODIFY_WRITE = 0x07,
        TN5250_COMMAND_ERASE_WRITE_ALTERNATE = 0x0D,
        TN5250_COMMAND_WRITE_TO_DISPLAY = 0x11,
        TN5250_COMMAND_CLEAR_UNIT = 0x40,
        TN5250_COMMAND_READ_MDT_FIELDS = 0x52,
    };

    constexpr CommandCode() : value(0) {}
    constexpr explicit CommandCode(uint16_t v) : value(v) {}

    /**
     * Human-readable description of this command code.
     *
     * @return A short string describing the command code; includes hex code for unknown command codes.
     */
    std::string description() const;
};

} // namespace tn5250::message::command
