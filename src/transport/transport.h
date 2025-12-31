#pragma once

// Transport layer - Network communication
// This layer contains:
// - Socket client (TN5250Client)
// - TN5250 handshake
// - Telnet options negotiation
// - TLS support
// - Protocol message encoding/decoding (ProtocolParser)

#include "tn5250_client.h"
#include "protocol_parser.h"
#include "telnet_options.h"

namespace transport {
    // All transport components are now implemented
}

