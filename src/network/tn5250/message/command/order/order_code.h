#pragma once

#include <cstdint>
#include <string>

namespace tn5250::message::command::order {

/**
 * TN5250 order code appearing in Write commands (RFC1205).
 */
struct OrderCode {
    uint8_t value;

    // Known TN5250 display orders (unscoped enum with fixed underlying type)
    // Source: IBM SA21-9247-6 - IBM 5250 Information Display System Functions Reference Manual, page 2-136
    enum : uint8_t {
        START_OF_HEADER = 0x01,
        REPEAT_TO_ADDRESS = 0x02,
        INSERT_CURSOR = 0x03,
        SET_BUFFER_ADDRESS = 0x11,
        START_FIELD = 0x1D
    };

    constexpr OrderCode() : value(0) {}
    constexpr explicit OrderCode(uint8_t v) : value(v) {}

    /**
     * Human-readable description of this order.
     *
     * @return A short string describing the order; includes hex code for unknown orders.
     */
    std::string description() const;
};

} // namespace tn5250::message::command::order
