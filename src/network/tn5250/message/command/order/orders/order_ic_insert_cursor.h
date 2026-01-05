#pragma once

#include "../order_base.h"
#include "utils/binary/binary.h"
#include "utils/hex/hex.h"
#include <iostream>
#include <string>
#include <vector>

namespace tn5250::message::command::order {

/**
 * Insert Cursor (IC) Order
 *
 * @see IBM SA21-9247-6 - IBM 5250 Information Display System Functions Reference Manual
 *
 * Format:
 *
 * IC Order   Bytes 1 and 2           Bytes 3 to ?
 *
 *       X'03'    Row Address   Column Address
 *
 * Function: This order sets the system insert cursor (IC) address to the location specified
 by the two bytes that follow the order when it is included in the Write to Display command
 or moves the cursor to the specified address without affecting the system IC address when
 it is included in Write Error Code command. Byte 1 gives the row address and byte 2 gives
 the column address. See the index entries home key and display, commands.
 *
 * Note: If there are more than one of these orders in the display station output data stream
 * (LU-LU commands from host system to controller for LU), only the last one is saved. It is
 * used as the home address (system IC address) for the Home function.
 *
 * Restrictions: A parameter error is posted when:
 * - There are fewer than two bytes following the order.
 * - The row address equals 0 or is greater than:
 *   - 24 for Model 12 (1920 characters) or
 *   - 12 for Model 2 (960 characters).
 * - The column address equals 0 or is greater than 80.
 *
 * Results: When the order is used in the Write to Display command, the cursor is not
 * immediately moved; the address is saved for later use. The cursor is moved when the entire
 * Write to Display is completed.
 *
 * When the order is used in the Write Error Code command, the cursor is moved to the address
 * given in the IC order and does not affect the system IC address. The cursor exits the field
 * regardless of the type and does not perform any field checks. For example, it does not check
 * for a filled field for a field specified as mandatory fill.
 */
struct OrderIcInsertCursor : OrderBase {
    uint8_t rowAddress;
    uint8_t columnAddress;

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
     *   <OrderIcInsertCursor>
     *    │ rowAddress    : 0x01 (1)
     *    │ columnAddress : 0x02 (2)
     *    └───
     *
     * @param out    Output stream to write to.
     * @param indent Indentation level for pretty-printing.
     */
    void describe(std::ostream &out, int indent) const;
};

} // namespace tn5250::message::command::order