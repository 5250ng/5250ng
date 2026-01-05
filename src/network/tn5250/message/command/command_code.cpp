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
    default: {
        std::ostringstream ss;
        ss << "Unknown (0x" << std::hex << std::nouppercase << std::setfill('0') << std::setw(2)
           << value << ")";
        return ss.str();
    }
    }
}

} // namespace tn5250::message::command
