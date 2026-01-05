#include "options.h"

#include <QString>
#include <cstdint>

namespace telnet {

QString telnetOptionToString(telnet::TelnetOption opt) {
    using telnet::TelnetOption;
    switch (opt) {
    case TelnetOption::TRANSMIT_BINARY:
        return "BINARY";
    case TelnetOption::ECHO:
        return "ECHO";
    case TelnetOption::SUPPRESS_GO_AHEAD:
        return "SUPPRESS_GO_AHEAD";
    case TelnetOption::TERMINAL_TYPE:
        return "TERMINAL_TYPE";
    case TelnetOption::END_OF_RECORD:
        return "EOR";
    case TelnetOption::NEGOTIATE_ABOUT_WINDOW_SIZE:
        return "NAWS";
    case TelnetOption::TERMINAL_SPEED:
        return "TERMINAL_SPEED";
    case TelnetOption::NEW_ENVIRON:
        return "NEW_ENVIRON";
    case TelnetOption::TN3270E:
        return "TN3270E";
    case TelnetOption::TELNET_START_TLS:
        return "START_TLS";
    default:
        return QString("UNKNOWN(0x%1)")
            .arg(static_cast<uint8_t>(opt), 2, 16, QChar('0'))
            .toUpper();
    }
}

} // namespace telnet