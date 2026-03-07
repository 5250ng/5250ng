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

#pragma once

#include "host_constants.h"
#include <QByteArray>
#include <QString>
#include <cstdint>

namespace hostserver {

// IBM i host server 20-byte header + packet builder/parser.
// All multi-byte fields are big-endian.
//
// Header layout:
//   [0-3]   Length (4 bytes, includes entire packet)
//   [4]     Client attributes
//   [5]     Server attributes
//   [6-7]   Server ID
//   [8-11]  CS Instance
//   [12-15] Correlation ID
//   [16-17] Template length
//   [18-19] Request/Reply ID
//   [20+]   Template data + optional LL/CP variable fields
class HostDataStream {
  public:
    static constexpr int HEADER_SIZE = 20;

    HostDataStream() = default;

    // Build a packet from scratch
    static QByteArray buildPacket(uint8_t clientAttr,
                                  uint8_t serverAttr,
                                  ServerID serverId,
                                  uint32_t correlation,
                                  uint16_t templateLength,
                                  uint16_t reqRepId,
                                  const QByteArray &payload);

    // Parse header fields from a raw packet (must be >= 20 bytes)
    static bool parseHeader(const QByteArray &data,
                            uint32_t &length,
                            uint8_t &clientAttr,
                            uint8_t &serverAttr,
                            uint16_t &serverId,
                            uint32_t &csInstance,
                            uint32_t &correlation,
                            uint16_t &templateLength,
                            uint16_t &reqRepId);

    // Read the 4-byte total packet length from the first 4 bytes
    static uint32_t packetLength(const QByteArray &data);

    // Convenience: read a big-endian uint16 at the given offset
    static uint16_t readU16(const QByteArray &data, int offset);
    // Convenience: read a big-endian uint32 at the given offset
    static uint32_t readU32(const QByteArray &data, int offset);
    // Convenience: read a big-endian uint64 at the given offset
    static uint64_t readU64(const QByteArray &data, int offset);

    // Convenience: write a big-endian uint16 into a QByteArray builder
    static void writeU16(QByteArray &buf, uint16_t v);
    // Convenience: write a big-endian uint32 into a QByteArray builder
    static void writeU32(QByteArray &buf, uint32_t v);
    // Convenience: write a big-endian uint64 into a QByteArray builder
    static void writeU64(QByteArray &buf, uint64_t v);

    // Build an LL/CP variable-length field: [4-byte LL][2-byte CP][data]
    // LL = data.size() + 6
    static QByteArray buildLLCP(uint16_t cp, const QByteArray &data);

    // Scan for LL/CP structures starting at offset, return data for the
    // first matching code point, or empty if not found
    static QByteArray findCodePoint(const QByteArray &data, int offset, uint16_t cp);

    // Encode a QString to UTF-16BE bytes (CCSID 1200)
    static QByteArray toUtf16BE(const QString &str);

    // Decode UTF-16BE bytes to QString
    static QString fromUtf16BE(const QByteArray &data);

    // Encode a user ID to 10-byte EBCDIC (CCSID 37), uppercase, padded with 0x40
    static QByteArray encodeUserId(const QString &userId);
};

} // namespace hostserver
