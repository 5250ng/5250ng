#pragma once

#include "command_code.h"
#include "order/order.h"
#include "utils/hex/hex.h"
#include <iostream>
#include <list>
#include <string>
#include <vector>

namespace tn5250::message::command {

struct CommandBase {
    CommandCode code;
    std::list<order::Order> orders;

    /**
     * Unmarshal the command from a byte buffer.
     *
     * @param buffer Input bytes; must contain at least the command code and orders.
     * @param error  Optional error string; set on failure.
     * @return true on success; false on invalid length or malformed input.
     */
    virtual uint32_t unmarshal(const std::vector<uint8_t> &buffer, std::string *error = nullptr) = 0;

    /**
     * Marshal the command to a byte buffer.
     *
     * @param error Optional error string; unused for now (reserved for future validation).
     * @return A vector containing the encoded command.
     */
    virtual std::vector<uint8_t> marshal(std::string *error = nullptr) const = 0;

    /**
     * Write a human-readable representation of the command to an output stream.
     *
     * @param out    Output stream to write to.
     * @param indent Indentation level for pretty-printing.
     */
    virtual void describe(std::ostream &out, int indent) const = 0;
};

} // namespace tn5250::message::command