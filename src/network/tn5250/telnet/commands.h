#pragma once

#include <QString>
#include <cstdint>

namespace telnet {

// Telnet command codes (RFC 854)
// TELNET COMMAND STRUCTURE
//
// All TELNET commands consist of at least a two byte sequence:  the
// "Interpret as Command" (IAC) escape character followed by the code
// for the command.  The commands dealing with option negotiation are
// three byte sequences, the third byte being the code for the option
// referenced.  This format was chosen so that as more comprehensive use
// of the "data space" is made -- by negotiations from the basic NVT, of
// course -- collisions of data bytes with reserved command values will
// be minimized, all such collisions requiring the inconvenience, and
// inefficiency, of "escaping" the data bytes into the stream.  With the
// current set-up, only the IAC need be doubled to be sent as data, and
// the other 255 codes may be passed transparently.
//
// The following are the defined TELNET commands.  Note that these codes
// and code sequences have the indicated meaning only when immediately
// preceded by an IAC.
enum class TelnetCommand : uint8_t {
    EOR = 239,  // End of Record (RFC 885). Marks end of a TN5250 GDS record.
    SE = 240,   // End of subnegotiation parameters.
    NOP = 241,  // No operation.
    DM = 242,   // Data Mark: The data stream portion of a Synch.
                // This should always be accompanied by a TCP Urgent notification.
    BRK = 243,  // NVT character BRK.
    IP = 244,   // The function IP.
    AO = 245,   // The function AO.
    AYT = 246,  // The function AYT.
    EC = 247,   // The function EC.
    EL = 248,   // The function EL.
    GA = 249,   // The GA signal.
    SB = 250,   // Indicates that what follows is subnegotiation of the indicated
                // option.
    WILL = 251, // Indicates the desire to begin performing, or confirmation that
                // you are now performing, the indicated option.
    WONT = 252, // Indicates the refusal to perform, or continue performing, the
                // indicated option.
    DO = 253,   // Indicates the request that the other party perform, or
                // confirmation that you are expecting the other party to perform,
                // the indicated option.
    DONT = 254, // Indicates the demand that the other party stop performing, or
                // confirmation that you are no longer expecting the other party
                // to perform, the indicated option.
    IAC = 255   // Data Byte 255.
};

/**
 * Determine whether the provided byte value corresponds to a TELNET command
 * code as defined by RFC 854 (values in the range 240..255).
 *
 * This function is constexpr and can be used in compile-time contexts.
 *
 * Examples:
 * - isTelnetCommand(240) -> true  (SE)
 * - isTelnetCommand(255) -> true  (IAC)
 * - isTelnetCommand(0x41) -> false ('A', ordinary data)
 *
 * Returns:
 * - true if byte is one of SE, NOP, DM, BRK, IP, AO, AYT, EC, EL, GA,
 *   SB, WILL, WONT, DO, DONT, or IAC; false otherwise.
 */
constexpr bool isTelnetCommand(uint8_t byte) {
    switch (byte) {
    case static_cast<uint8_t>(TelnetCommand::EOR):
    case static_cast<uint8_t>(TelnetCommand::SE):
    case static_cast<uint8_t>(TelnetCommand::NOP):
    case static_cast<uint8_t>(TelnetCommand::DM):
    case static_cast<uint8_t>(TelnetCommand::BRK):
    case static_cast<uint8_t>(TelnetCommand::IP):
    case static_cast<uint8_t>(TelnetCommand::AO):
    case static_cast<uint8_t>(TelnetCommand::AYT):
    case static_cast<uint8_t>(TelnetCommand::EC):
    case static_cast<uint8_t>(TelnetCommand::EL):
    case static_cast<uint8_t>(TelnetCommand::GA):
    case static_cast<uint8_t>(TelnetCommand::SB):
    case static_cast<uint8_t>(TelnetCommand::WILL):
    case static_cast<uint8_t>(TelnetCommand::WONT):
    case static_cast<uint8_t>(TelnetCommand::DO):
    case static_cast<uint8_t>(TelnetCommand::DONT):
    case static_cast<uint8_t>(TelnetCommand::IAC):
        return true;
    default:
        return false;
    }
}

// Overload taking a TelnetCommand argument
constexpr bool isTelnetCommand(TelnetCommand cmd) {
    return isTelnetCommand(static_cast<uint8_t>(cmd));
}

/**
 * Determine whether a TELNET command is a standalone, single-byte command
 * (i.e., does not take an option byte). This matches the checks in client code
 * that treat these commands specially while parsing (SE, NOP, DM, BRK, IP,
 * AO, AYT, EC, EL, GA).
 */
constexpr bool isStandaloneTelnetCommand(uint8_t byte) {
    switch (byte) {
    case static_cast<uint8_t>(TelnetCommand::EOR):
    case static_cast<uint8_t>(TelnetCommand::SE):
    case static_cast<uint8_t>(TelnetCommand::NOP):
    case static_cast<uint8_t>(TelnetCommand::DM):
    case static_cast<uint8_t>(TelnetCommand::BRK):
    case static_cast<uint8_t>(TelnetCommand::IP):
    case static_cast<uint8_t>(TelnetCommand::AO):
    case static_cast<uint8_t>(TelnetCommand::AYT):
    case static_cast<uint8_t>(TelnetCommand::EC):
    case static_cast<uint8_t>(TelnetCommand::EL):
    case static_cast<uint8_t>(TelnetCommand::GA):
        return true;
    default:
        return false;
    }
}

// Overload taking a TelnetCommand argument
constexpr bool isStandaloneTelnetCommand(TelnetCommand cmd) {
    return isStandaloneTelnetCommand(static_cast<uint8_t>(cmd));
}

QString telnetCommandToString(TelnetCommand cmd);

} // namespace telnet
