#pragma once

#include "../command_base.h"
#include "../order/order.h"
#include "utils/binary/binary.h"
#include "utils/hex/hex.h"
#include <cstdint>
#include <iostream>
#include <list>
#include <string>
#include <vector>

namespace tn5250::message::command {

struct CommandCsClearScreen : CommandBase {
    /**
     * Unmarshal the command from a byte buffer.
     *
     * @param buffer Input bytes; must contain at least the command code and orders.
     * @param error  Optional error string; set on failure.
     * @return true on success; false on invalid length or malformed input.
     */
    uint32_t unmarshal(const std::vector<uint8_t> &buffer, std::string *error = nullptr);

    /**
     * Marshal the command to a byte buffer.
     *
     * @param error Optional error string; unused for now (reserved for future validation).
     * @return A vector containing the encoded command.
     */
    std::vector<uint8_t> marshal(std::string *error = nullptr) const;

    /**
     * Write a human-readable representation of the command to an output stream.
     *
     * @param out    Output stream to write to.
     * @param indent Indentation level for pretty-printing.
     */
    void describe(std::ostream &out, int indent) const;
};

} // namespace tn5250::message::command