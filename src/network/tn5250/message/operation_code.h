#pragma once

#include <cstdint>
#include <string>

namespace tn5250::message {

/**
 * Represents a TN5250 operation code present in the variable GDS header.
 */
struct OperationCode {
    uint8_t value;

    constexpr OperationCode() : value(0) {}
    constexpr explicit OperationCode(uint8_t v) : value(v) {}

    // Known TN5250 operation codes (unscoped enum with fixed underlying type)
    enum : uint8_t {
        TN5250_OPCODE_NOP = 0x00,
        TN5250_OPCODE_INVITE = 0x01,
        TN5250_OPCODE_OUTPUT_ONLY = 0x02,
        TN5250_OPCODE_PUT_GET = 0x03,
        TN5250_OPCODE_SAVE_SCREEN = 0x04,
        TN5250_OPCODE_RESTORE_SCREEN = 0x05,
        TN5250_OPCODE_READ_IMMEDIATE = 0x06,
        TN5250_OPCODE_RESERVED_07 = 0x07,
        TN5250_OPCODE_READ_SCREEN = 0x08,
        TN5250_OPCODE_RESERVED_09 = 0x09,
        TN5250_OPCODE_CANCEL_INVITE = 0x0A,
        TN5250_OPCODE_MESSAGE_LIGHT_ON = 0x0B,
        TN5250_OPCODE_MESSAGE_LIGHT_OFF = 0x0C,
    };

    constexpr bool isNOP() const { return value == TN5250_OPCODE_NOP; }
    constexpr bool isInvite() const { return value == TN5250_OPCODE_INVITE; }
    constexpr bool isOutputOnly() const { return value == TN5250_OPCODE_OUTPUT_ONLY; }
    constexpr bool isPutGet() const { return value == TN5250_OPCODE_PUT_GET; }
    constexpr bool isSaveScreen() const { return value == TN5250_OPCODE_SAVE_SCREEN; }
    constexpr bool isRestoreScreen() const { return value == TN5250_OPCODE_RESTORE_SCREEN; }
    constexpr bool isReadImmediate() const { return value == TN5250_OPCODE_READ_IMMEDIATE; }
    constexpr bool isReadScreen() const { return value == TN5250_OPCODE_READ_SCREEN; }
    constexpr bool isCancelInvite() const { return value == TN5250_OPCODE_CANCEL_INVITE; }
    constexpr bool isMessageLightOn() const { return value == TN5250_OPCODE_MESSAGE_LIGHT_ON; }
    constexpr bool isMessageLightOff() const { return value == TN5250_OPCODE_MESSAGE_LIGHT_OFF; }

    /**
     * Human-readable description of this operation code.
     *
     * @return A short string describing the opcode; includes hex code for unknown values.
     */
    std::string description() const;
};

} // namespace tn5250::message
