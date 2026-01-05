#pragma once

#include <QString>
#include <cstdint>

namespace telnet {

// Telnet option codes (IANA registry)
// Source: https://www.iana.org/assignments/telnet-options/telnet-options.xhtml
// and referenced RFCs.
enum class TelnetOption : uint8_t {
    TRANSMIT_BINARY = 0,   // Binary Transmission [RFC856]
    ECHO = 1,              // Echo [RFC857]
    RECONNECTION = 2,      // Reconnection [NIC 15391 of 1973]
    SUPPRESS_GO_AHEAD = 3, // Suppress Go Ahead [RFC858]
    APPROX_MESSAGE_SIZE_NEGOTIATION =
        4,                            // Approx Message Size Negotiation [NIC 15393 of 1973]
    STATUS = 5,                       // Status [RFC859]
    TIMING_MARK = 6,                  // Timing Mark [RFC860]
    REMOTE_CONTROLLED_TRANS_ECHO = 7, // Remote Controlled Trans and Echo [RFC726]
    OUTPUT_LINE_WIDTH = 8,            // Output Line Width [NIC 20196 of Aug 1978]
    OUTPUT_PAGE_SIZE = 9,             // Output Page Size [NIC 20197 of Aug 1978]
    OUTPUT_CR_DISPOSITION = 10,       // Output Carriage-Return Disposition [RFC652]
    OUTPUT_HORIZONTAL_TAB_STOPS = 11, // Output Horizontal Tab Stops [RFC653]
    OUTPUT_HORIZONTAL_TAB_DISPOSITION =
        12,                           // Output Horizontal Tab Disposition [RFC654]
    OUTPUT_FORMFEED_DISPOSITION = 13, // Output Formfeed Disposition [RFC655]
    OUTPUT_VERTICAL_TABSTOPS = 14,    // Output Vertical Tabstops [RFC656]
    OUTPUT_VERTICAL_TAB_DISPOSITION =
        15,                           // Output Vertical Tab Disposition [RFC657]
    OUTPUT_LINEFEED_DISPOSITION = 16, // Output Linefeed Disposition [RFC658]
    EXTENDED_ASCII = 17,              // Extended ASCII [RFC698]
    LOGOUT = 18,                      // Logout [RFC727]
    BYTE_MACRO = 19,                  // Byte Macro [RFC735]
    DATA_ENTRY_TERMINAL = 20,         // Data Entry Terminal [RFC1043][RFC732]
    SUPDUP = 21,                      // SUPDUP [RFC736][RFC734]
    SUPDUP_OUTPUT = 22,               // SUPDUP Output [RFC749]
    SEND_LOCATION = 23,               // Send Location [RFC779]
    TERMINAL_TYPE = 24,               // Terminal Type [RFC1091]
    END_OF_RECORD = 25,               // End of Record [RFC885]
    TACACS_USER_IDENTIFICATION = 26,  // TACACS User Identification [RFC927]
    OUTPUT_MARKING = 27,              // Output Marking [RFC933]
    TERMINAL_LOCATION_NUMBER = 28,    // Terminal Location Number [RFC946]
    TELNET_3270_REGIME = 29,          // Telnet 3270 Regime [RFC1041]
    X3_PAD = 30,                      // X.3 PAD [RFC1053]
    NEGOTIATE_ABOUT_WINDOW_SIZE = 31, // Negotiate About Window Size [RFC1073]
    TERMINAL_SPEED = 32,              // Terminal Speed [RFC1079]
    REMOTE_FLOW_CONTROL = 33,         // Remote Flow Control [RFC1372]
    LINEMODE = 34,                    // Linemode [RFC1184]
    X_DISPLAY_LOCATION = 35,          // X Display Location [RFC1096]
    ENVIRONMENT = 36,                 // Environment Option [RFC1408]
    AUTHENTICATION = 37,              // Authentication Option [RFC2941]
    ENCRYPTION = 38,                  // Encryption Option [RFC2946]
    NEW_ENVIRON = 39,                 // New Environment Option [RFC1572]
    TN3270E = 40,                     // TN3270E [RFC2355]
    XAUTH = 41,                       // XAUTH [Rob_Earhart]
    CHARSET = 42,                     // CHARSET [RFC2066]
    TELNET_REMOTE_SERIAL_PORT =
        43,                          // Telnet Remote Serial Port (RSP) [Robert_Barnes]
    COM_PORT_CONTROL = 44,           // Com Port Control Option [RFC2217]
    TELNET_SUPPRESS_LOCAL_ECHO = 45, // Telnet Suppress Local Echo [Wirt_Atmar]
    TELNET_START_TLS = 46,           // Telnet Start TLS [Michael_Boe]
    KERMIT = 47,                     // KERMIT [RFC2840]
    SEND_URL = 48,                   // SEND-URL [David_Croft]
    FORWARD_X = 49,                  // FORWARD_X [Jeffrey_Altman]
    // 50-137 Unassigned [IANA]
    TELOPT_PRAGMA_LOGON = 138,     // TELOPT PRAGMA LOGON [Steve_McGregory]
    TELOPT_SSPI_LOGON = 139,       // TELOPT SSPI LOGON [Steve_McGregory]
    TELOPT_PRAGMA_HEARTBEAT = 140, // TELOPT PRAGMA HEARTBEAT [Steve_McGregory]
    // 141-254 Unassigned
    EXTENDED_OPTIONS_LIST = 255, // Extended-Options-List [RFC861]
};

// String helpers
QString telnetOptionToString(TelnetOption opt);

} // namespace telnet
