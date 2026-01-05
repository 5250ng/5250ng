#include "order_code.h"
#include <iomanip>
#include <sstream>

namespace tn5250::message::command::order {

/**
 * Human-readable description of this order.
 *
 * @return A short string describing the order; includes hex code for unknown orders.
 */
std::string OrderCode::description() const {
    switch (value) {
    case START_OF_HEADER:
        return "Start Of Header";
    case REPEAT_TO_ADDRESS:
        return "Repeat To Address";
    case INSERT_CURSOR:
        return "Insert Cursor";
    case SET_BUFFER_ADDRESS:
        return "Set Buffer Address";
    case START_FIELD:
        return "Start Field";
    default: {
        std::ostringstream ss;
        ss << "Unknown (0x" << std::hex << std::nouppercase << std::setfill('0') << std::setw(2)
           << value << ")";
        return ss.str();
    }
    }
}

} // namespace tn5250::message::command::order