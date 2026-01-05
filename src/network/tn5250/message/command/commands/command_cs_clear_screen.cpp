#include "command_cs_clear_screen.h"

namespace tn5250::message::command {

/**
 * Unmarshal the command from a byte buffer.
 *
 * @param buffer Input bytes; must contain at least the command code.
 * @param error  Optional error string; set on failure.
 * @return true on success; false on invalid length or malformed input.
 */
uint32_t CommandCsClearScreen::unmarshal(const std::vector<uint8_t> &buffer, std::string *error) {
    if (buffer.size() < 2) {
        if (error)
            *error = "5250Command: buffer too short for ESC and command code";
        return 0;
    }

    uint32_t read_bytes = 0;
    orders.clear();

    // Verify the escape byte is present
    uint8_t escape_byte = buffer[read_bytes];
    if (escape_byte != 0x04) {
        if (error) {
            *error = "5250Command: escape byte is not 0x04";
        }
        return 0;
    }
    read_bytes++;

    // Verify the command code is correct
    code = CommandCode(buffer[read_bytes]);
    read_bytes++;
    if (code.value != CommandCode::TN5250_COMMAND_CLEAR_UNIT) {
        if (error) {
            *error = "5250Command: command code is not TN5250_COMMAND_CLEAR_UNIT (0x40)";
        }
        return 0;
    }

    return read_bytes;
};

/**
 * Marshal the command to a byte buffer.
 *
 * @param error Optional error string; unused for now (reserved for future validation).
 * @return A vector containing the encoded command.
 */
std::vector<uint8_t> CommandCsClearScreen::marshal(std::string *error) const {
    std::vector<uint8_t> buffer;

    buffer.push_back(0x04);
    buffer.push_back(CommandCode::TN5250_COMMAND_CLEAR_UNIT);

    return buffer;
};

/**
 * Write a human-readable representation of the command to an output stream.
 *
 * Example:
 *   <CommandCsClearScreen>
 *    └───
 *
 * @param out    Output stream to write to.
 * @param indent Indentation level for pretty-printing.
 */
void CommandCsClearScreen::describe(std::ostream &out, int indent) const {
    std::string indentPrompt;
    indentPrompt.reserve(static_cast<size_t>(indent) * 3);
    for (int i = 0; i < std::max(0, indent); ++i) {
        indentPrompt += "  │ ";
    }

    out << indentPrompt << "<CommandCsClearScreen>\n";
    out << indentPrompt << "  │ code     : 0x"
        << utils::hex::to_hex_string_padded_2(CommandCode::TN5250_COMMAND_CLEAR_UNIT)
        << " (" << CommandCode(CommandCode::TN5250_COMMAND_CLEAR_UNIT).description() << ")"
        << "\n";
    out << indentPrompt << "  └───\n";
}

} // namespace tn5250::message::command