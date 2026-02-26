#include "command_code.h"
#include <iomanip>
#include <sstream>

namespace tn5250::message::command {

/**
 * Human-readable description of this command code.
 *
 * @return A short string describing the command code; includes hex code for unknown command codes.
 */
std::string CommandCode::description() const {
    switch (value) {
    case TN5250_COMMAND_WRITE_TO_DISPLAY:
        return "Write To Display";
    case TN5250_COMMAND_ERASE_WRITE:
        return "Erase Write";
    case TN5250_COMMAND_ERASE_WRITE_ALTERNATE:
        return "Erase Write Alternate";
    case TN5250_COMMAND_READ_MODIFY:
        return "Read Modify";
    case TN5250_COMMAND_CLEAR_UNIT:
        return "Clear Unit";
    case TN5250_COMMAND_CLEAR_UNIT_ALTERNATE:
        return "Clear Unit Alternate";
    case TN5250_COMMAND_WRITE_ERROR_CODE:
        return "Write Error Code";
    case TN5250_COMMAND_ROLL:
        return "Roll";
    case TN5250_COMMAND_READ_INPUT_FIELDS:
        return "Read Input Fields";
    case TN5250_COMMAND_CLEAR_FORMAT_TABLE:
        return "Clear Format Table";
    case TN5250_COMMAND_READ_MDT_FIELDS:
        return "Read MDT Fields";
    case TN5250_COMMAND_READ_MODIFY_WRITE:
        return "Read Modify Write";
    case TN5250_COMMAND_READ_IMMEDIATE:
        return "Read Immediate";
    default: {
        std::ostringstream ss;
        ss << "Unknown (0x" << std::hex << std::nouppercase << std::setfill('0') << std::setw(2)
           << value << ")";
        return ss.str();
    }
    }
}

} // namespace tn5250::message::command
