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

#include <QByteArray>
#include <QString>
#include <QVector>
#include <cstdint>

namespace core::pcap {

// One host→client TN5250 GDS record extracted from a capture. The telnet
// layer (IAC negotiation, subnegotiations, IAC IAC escaping, IAC EOR record
// terminators) has already been removed: `data` is exactly what the live
// client would emit via TN5250Client::dataReceived for one record.
struct ReplayRecord {
    qint64 timestampUsec = 0; // capture timestamp of the packet that completed the record
    QByteArray data;          // raw GDS record bytes
};

// A replayable TN5250 session extracted from a capture file.
struct ReplaySession {
    QVector<ReplayRecord> records;
    QString serverEndpoint; // "address:port" of the AS/400 side
    QString clientEndpoint; // "address:port" of the terminal side
};

// Loads a TN5250 session from a packet capture.
//
// Supported containers: classic pcap (both endiannesses, microsecond and
// nanosecond timestamp variants) and pcapng (Section Header / Interface
// Description / Enhanced Packet / Simple Packet blocks).
// Supported link layers: Ethernet (including 802.1Q/802.1ad VLAN tags),
// Linux cooked SLL and SLL2, raw IP, and BSD loopback (NULL/LOOP).
// IPv4 and IPv6 are supported; fragmented IP packets are ignored.
//
// The loader reassembles every TCP flow in the capture, selects the flow
// that carries telnet/TN5250 traffic (and within it the host→client
// direction), strips the telnet layer, and splits the application stream
// into GDS records at IAC EOR boundaries.
class PcapReplayLoader {
  public:
    // Reads `path` and extracts the session. Returns false and sets *error
    // (if non-null) when the file cannot be read, is not a recognized
    // capture format, or contains no TN5250 session data.
    static bool loadFile(const QString &path, ReplaySession *out, QString *error);

    // Same as loadFile but operates on an in-memory capture image.
    static bool loadData(const QByteArray &data, ReplaySession *out, QString *error);
};

} // namespace core::pcap
