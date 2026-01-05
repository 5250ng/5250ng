#include "record_type.h"
#include <iomanip>
#include <sstream>

namespace tn5250::message::header {

std::string RecordType::description() const {
    switch (value) {
    case SNA_RECORD_TYPE_GENERAL_DATA_STREAM:
        return "General Data Stream";
    default: {
        std::ostringstream ss;
        ss << "Unknown (0x" << std::hex << std::nouppercase << std::setfill('0') << std::setw(4)
           << value << ")";
        return ss.str();
    }
    }
}

} // namespace tn5250::message::header
