// 5250ng - A modern IBM TN5250 terminal emulator                                                                                                                                                            
// Copyright (C) 2025-2026 Remi GASCOU (Podalirius)                                                                                                                                                          
//                                                                                                                                                                                                           
// This program is free software: you can redistribute it and/or modify                                                                                                                                      
// it under the terms of the GNU General Public License as published by                                                                                                                                      
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.                                                                                                                                                                       
//                                                                                                                                                                                                           
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "commands.h"

#include <QString>

namespace telnet {

// Helper functions
//
// The utilities below aid in identifying TELNET command bytes as defined
// by RFC 854. They are intended for use while parsing a byte stream to
// distinguish TELNET control sequences from data bytes.
//
// Note:
// - TELNET commands have meaning only when immediately preceded by IAC (255).
// - A literal data 255 byte in the stream must be sent as IAC IAC.
// - These helpers do not validate multi-byte command sequences; they only
//   classify a single byte value.

/**
 * Converts a TelnetCommand enum value to its human-readable string
 * representation.
 *
 * This function maps each command code in the telnet::TelnetCommand enumeration
 * to its symbolic name as defined in RFC 854 (e.g., "SE", "NOP", "WILL", etc.).
 * This is useful for logging, debugging, and displaying Telnet command bytes
 * in a readable format. If the provided command code is not recognized, the
 * function returns a string of the form "UNKNOWN(0xNN)", where NN is the
 * hexadecimal byte value.
 *
 * Example:
 *   telnetCommandToString(TelnetCommand::WILL)    // returns "WILL"
 *   telnetCommandToString((TelnetCommand)0xFF)    // returns "IAC"
 *   telnetCommandToString((TelnetCommand)0xA0)    // returns "UNKNOWN(0xA0)"
 *
 * @param cmd The TelnetCommand value to convert.
 * @return QString The symbolic name for the command, or a hex code string if
 * unknown.
 */
QString telnetCommandToString(telnet::TelnetCommand cmd) {
    using telnet::TelnetCommand;
    switch (cmd) {
    case TelnetCommand::EOR:
        return "EOR";
    case TelnetCommand::SE:
        return "SE";
    case TelnetCommand::NOP:
        return "NOP";
    case TelnetCommand::DM:
        return "DM";
    case TelnetCommand::BRK:
        return "BRK";
    case TelnetCommand::IP:
        return "IP";
    case TelnetCommand::AO:
        return "AO";
    case TelnetCommand::AYT:
        return "AYT";
    case TelnetCommand::EC:
        return "EC";
    case TelnetCommand::EL:
        return "EL";
    case TelnetCommand::GA:
        return "GA";
    case TelnetCommand::SB:
        return "SB";
    case TelnetCommand::WILL:
        return "WILL";
    case TelnetCommand::WONT:
        return "WONT";
    case TelnetCommand::DO:
        return "DO";
    case TelnetCommand::DONT:
        return "DONT";
    case TelnetCommand::IAC:
        return "IAC";
    default:
        return QString("UNKNOWN(0x%1)")
            .arg(static_cast<uint8_t>(cmd), 2, 16, QChar('0'))
            .toUpper();
    }
}

/**
 * Check whether the provided byte is the IAC (Interpret As Command) byte (255).
 *
 * According to RFC 854, IAC introduces TELNET command sequences. A literal
 * data value of 255 must be represented as the two-byte sequence IAC IAC.
 *
 * Returns:
 * - true if byte == 255 (IAC), false otherwise.
 */
constexpr bool isIAC(uint8_t byte) {
    return byte == static_cast<uint8_t>(TelnetCommand::IAC);
}

} // namespace telnet