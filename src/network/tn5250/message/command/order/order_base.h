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