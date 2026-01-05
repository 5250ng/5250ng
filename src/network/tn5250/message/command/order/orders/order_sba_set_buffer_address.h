#pragma once

#include "../order_base.h"
#include "utils/binary/binary.h"
#include "utils/hex/hex.h"
#include <iostream>
#include <string>
#include <vector>

namespace tn5250::message::command::order {

/**
 * Set Buffer Address (SBA) Order
 *
 * @see IBM SA21-9247-6 - IBM 5250 Information Display System Functions Reference Manual
 *
 * Format:
 *
 * SBA Order   Bytes 1 and 2           Bytes 3 to ?
 *
 *       X'11'    Row Address   Column Address   Repeated Character
 *
 * Function:
 *  - Read: Used as a delimiter between fields that are sent back to the host system in response to
 *    the Read MDT command. See the index entry read MDT fields command.
 *  - Write: Used to set the current display address and thereby determine where the data display or
 *    field definition begins. Two bytes that follow this order tell the 5251 Models 2 or 12 this
 *    information.
 *
 * Restrictions:
 * A parameter error is posted when:
 * - There are fewer than 2 bytes following the order.
 * - The row address is equal to 0 or greater than 12 for Model 2 (960 characters) or 24 for Model
 *    12 (1920 characters).
 * - The column size is equal to 0 or greater than 80.
 *
 * Default: When the SBA is not specified in the Write to Display, the data starts at row 1, column 1
 * because this is where the Write to Display command initializes it. See the index entry write to
 * display command.
 *
 * Format:
 *
 * SBA Order   Bytes 1 and 2           Bytes 3 to ?
 *
 *       X'11'    Row Address   Column Address   Repeated Character
 */

struct OrderSbaSetBufferAddress : OrderBase {
    uint8_t rowAddress;
    uint8_t columnAddress;
    std::string repeatedCharacter;

    /**
     * Unmarshal the order from a byte buffer.
     *
     * @param buffer Input bytes; must contain at least the order code and attributes.
     * @param error  Optional error string; set on failure.
     * @return bytes read on success; 0 on failure.
     */
    uint32_t unmarshal(const std::vector<uint8_t> &buffer, std::string *error = nullptr);

    /**
     * Marshal the order to a byte buffer.
     *
     * @param error Optional error string; unused for now (reserved for future validation).
     * @return A vector containing the encoded order.
     */
    std::vector<uint8_t> marshal(std::string *error = nullptr) const;

    /**
     * Write a human-readable representation of the order to an output stream.
     *
     * Example:
     *   <OrderSbaSetBufferAddress>
     *    │ rowAddress    : 0x01 (1)
     *    │ columnAddress : 0x02 (2)
     *    │ repeatedCharacter : ["\u0080"]
     *    └───
     *
     * @param out    Output stream to write to.
     * @param indent Indentation level for pretty-printing.
     */
    void describe(std::ostream &out, int indent) const;
};
} // namespace tn5250::message::command::order