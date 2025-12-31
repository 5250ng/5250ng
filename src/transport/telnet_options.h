#pragma once

#include <cstdint>

namespace transport {

// Telnet command codes (RFC 854)
enum class TelnetCommand : uint8_t {
    SE   = 240,  // End of subnegotiation
    NOP  = 241,  // No operation
    DM   = 242,  // Data mark
    BRK  = 243,  // Break
    IP   = 244,  // Interrupt process
    AO   = 245,  // Abort output
    AYT  = 246,  // Are you there
    EC   = 247,  // Erase character
    EL   = 248,  // Erase line
    GA   = 249,  // Go ahead
    SB   = 250,  // Subnegotiation begin
    WILL = 251,  // Will
    WONT = 252,  // Won't
    DO   = 253,  // Do
    DONT = 254,  // Don't
    IAC  = 255   // Interpret as command
};

// Telnet option codes
enum class TelnetOption : uint8_t {
    BINARY           = 0,   // Binary transmission
    ECHO             = 1,   // Echo
    SUPPRESS_GO_AHEAD = 3,  // Suppress go ahead
    TERMINAL_TYPE    = 24,  // Terminal type
    EOR              = 25,  // End of record
    NAWS             = 31,  // Negotiate about window size
    TERMINAL_SPEED   = 32,  // Terminal speed
    NEW_ENVIRON      = 39,  // New environment
    TN3270E          = 40,  // TN3270E
    START_TLS        = 46   // Start TLS
};

// Helper functions
constexpr bool isTelnetCommand(uint8_t byte) {
    return byte >= 240 && byte <= 255;
}

constexpr bool isIAC(uint8_t byte) {
    return byte == static_cast<uint8_t>(TelnetCommand::IAC);
}

} // namespace transport

