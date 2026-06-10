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

#include "pcap_replay.h"
#include "network/tn5250_qt/telnet/commands.h"
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <algorithm>

namespace core::pcap {

namespace {

using telnet::TelnetCommand;

// ---------------------------------------------------------------------------
// Endian-aware integer reads
// ---------------------------------------------------------------------------

quint16 readU16(const uchar *p, bool bigEndian) {
    return bigEndian ? static_cast<quint16>((p[0] << 8) | p[1])
                     : static_cast<quint16>((p[1] << 8) | p[0]);
}

quint32 readU32(const uchar *p, bool bigEndian) {
    return bigEndian
        ? (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3])
        : (quint32(p[3]) << 24) | (quint32(p[2]) << 16) | (quint32(p[1]) << 8) | quint32(p[0]);
}

// Network byte order (big-endian) reads used by all protocol headers
quint16 readBE16(const uchar *p) { return readU16(p, true); }
quint32 readBE32(const uchar *p) { return readU32(p, true); }

// ---------------------------------------------------------------------------
// Capture container parsing
// ---------------------------------------------------------------------------

// Link-layer types (https://www.tcpdump.org/linktypes.html)
constexpr quint32 LINKTYPE_NULL = 0;
constexpr quint32 LINKTYPE_ETHERNET = 1;
constexpr quint32 LINKTYPE_RAW = 101;
constexpr quint32 LINKTYPE_LOOP = 108;
constexpr quint32 LINKTYPE_LINUX_SLL = 113;
constexpr quint32 LINKTYPE_LINUX_SLL2 = 276;

struct RawPacket {
    qint64 tsUsec = 0;
    quint32 linkType = LINKTYPE_ETHERNET;
    QByteArray frame;
};

// Classic pcap magic numbers as read little-endian from the file
constexpr quint32 PCAP_MAGIC_USEC = 0xA1B2C3D4;      // little-endian file, µs
constexpr quint32 PCAP_MAGIC_USEC_SWAP = 0xD4C3B2A1; // big-endian file, µs
constexpr quint32 PCAP_MAGIC_NSEC = 0xA1B23C4D;      // little-endian file, ns
constexpr quint32 PCAP_MAGIC_NSEC_SWAP = 0x4D3CB2A1; // big-endian file, ns

bool isClassicPcap(const QByteArray &data) {
    if (data.size() < 4) return false;
    quint32 magic = readU32(reinterpret_cast<const uchar *>(data.constData()), false);
    return magic == PCAP_MAGIC_USEC || magic == PCAP_MAGIC_USEC_SWAP
        || magic == PCAP_MAGIC_NSEC || magic == PCAP_MAGIC_NSEC_SWAP;
}

bool parseClassicPcap(const QByteArray &data, QVector<RawPacket> *out, QString *error) {
    const uchar *d = reinterpret_cast<const uchar *>(data.constData());
    const qsizetype size = data.size();
    if (size < 24) {
        if (error) *error = QStringLiteral("pcap file truncated (no global header)");
        return false;
    }
    quint32 magic = readU32(d, false);
    bool bigEndian = (magic == PCAP_MAGIC_USEC_SWAP || magic == PCAP_MAGIC_NSEC_SWAP);
    bool nanosecond = (magic == PCAP_MAGIC_NSEC || magic == PCAP_MAGIC_NSEC_SWAP);
    quint32 linkType = readU32(d + 20, bigEndian);

    qsizetype pos = 24;
    // Tolerate a capture cut off mid-record (live capture killed): keep the
    // packets parsed so far instead of failing the whole load.
    while (pos + 16 <= size) {
        quint32 tsSec = readU32(d + pos, bigEndian);
        quint32 tsFrac = readU32(d + pos + 4, bigEndian);
        quint32 inclLen = readU32(d + pos + 8, bigEndian);
        pos += 16;
        if (inclLen > static_cast<quint64>(size - pos)) break;
        RawPacket pkt;
        pkt.tsUsec = qint64(tsSec) * 1000000
                   + (nanosecond ? qint64(tsFrac) / 1000 : qint64(tsFrac));
        pkt.linkType = linkType;
        pkt.frame = data.mid(pos, inclLen);
        out->append(pkt);
        pos += inclLen;
    }
    return true;
}

// pcapng block types
constexpr quint32 PCAPNG_SHB = 0x0A0D0D0A;
constexpr quint32 PCAPNG_IDB = 0x00000001;
constexpr quint32 PCAPNG_SPB = 0x00000003;
constexpr quint32 PCAPNG_EPB = 0x00000006;
constexpr quint32 PCAPNG_BYTE_ORDER_MAGIC = 0x1A2B3C4D;

bool isPcapNg(const QByteArray &data) {
    if (data.size() < 12) return false;
    const uchar *d = reinterpret_cast<const uchar *>(data.constData());
    // SHB block type is an endianness palindrome
    return readU32(d, true) == PCAPNG_SHB
        && (readU32(d + 8, true) == PCAPNG_BYTE_ORDER_MAGIC
            || readU32(d + 8, false) == PCAPNG_BYTE_ORDER_MAGIC);
}

struct PcapNgInterface {
    quint32 linkType = LINKTYPE_ETHERNET;
    quint64 ticksPerSecond = 1000000; // from if_tsresol, default 10^-6
};

bool parsePcapNg(const QByteArray &data, QVector<RawPacket> *out, QString *error) {
    const uchar *d = reinterpret_cast<const uchar *>(data.constData());
    const qsizetype size = data.size();
    bool bigEndian = false;
    QVector<PcapNgInterface> interfaces;
    qint64 lastTsUsec = 0;
    qsizetype pos = 0;

    while (pos + 12 <= size) {
        quint32 blockType = readU32(d + pos, bigEndian);
        if (blockType == PCAPNG_SHB) {
            // New section: re-resolve endianness from the byte-order magic
            if (readU32(d + pos + 8, true) == PCAPNG_BYTE_ORDER_MAGIC) {
                bigEndian = true;
            } else if (readU32(d + pos + 8, false) == PCAPNG_BYTE_ORDER_MAGIC) {
                bigEndian = false;
            } else {
                if (error) *error = QStringLiteral("pcapng: bad byte-order magic");
                return false;
            }
            blockType = readU32(d + pos, bigEndian);
            interfaces.clear();
        }
        quint32 totalLen = readU32(d + pos + 4, bigEndian);
        // Block lengths are 4-aligned and include the 12-byte envelope
        if (totalLen < 12 || (totalLen & 3) != 0
            || totalLen > static_cast<quint64>(size - pos)) {
            break; // truncated or corrupt tail: keep what we have
        }
        const uchar *body = d + pos + 8;
        const quint32 bodyLen = totalLen - 12;

        if (blockType == PCAPNG_IDB && bodyLen >= 8) {
            PcapNgInterface iface;
            iface.linkType = readU16(body, bigEndian);
            // Walk options looking for if_tsresol (code 9, length 1)
            quint32 opt = 8;
            while (opt + 4 <= bodyLen) {
                quint16 code = readU16(body + opt, bigEndian);
                quint16 len = readU16(body + opt + 2, bigEndian);
                if (code == 0) break; // opt_endofopt
                if (opt + 4 + len > bodyLen) break;
                if (code == 9 && len == 1) {
                    quint8 res = body[opt + 4];
                    quint64 ticks = 1;
                    if (res & 0x80) {
                        for (int i = 0; i < (res & 0x7F) && i < 63; ++i) ticks <<= 1;
                    } else {
                        for (int i = 0; i < res && i < 19; ++i) ticks *= 10;
                    }
                    iface.ticksPerSecond = ticks;
                }
                opt += 4 + ((len + 3u) & ~3u);
            }
            interfaces.append(iface);
        } else if (blockType == PCAPNG_EPB && bodyLen >= 20) {
            quint32 ifId = readU32(body, bigEndian);
            quint64 ts = (quint64(readU32(body + 4, bigEndian)) << 32)
                       | readU32(body + 8, bigEndian);
            quint32 capLen = readU32(body + 12, bigEndian);
            if (capLen <= bodyLen - 20) {
                PcapNgInterface iface = (ifId < quint32(interfaces.size()))
                    ? interfaces[ifId] : PcapNgInterface{};
                RawPacket pkt;
                pkt.tsUsec = (iface.ticksPerSecond == 1000000)
                    ? qint64(ts)
                    : qint64((long double)ts * 1000000.0L / iface.ticksPerSecond);
                pkt.linkType = iface.linkType;
                pkt.frame = QByteArray(reinterpret_cast<const char *>(body + 20), capLen);
                out->append(pkt);
                lastTsUsec = pkt.tsUsec;
            }
        } else if (blockType == PCAPNG_SPB && bodyLen >= 4) {
            quint32 origLen = readU32(body, bigEndian);
            quint32 capLen = qMin<quint32>(origLen, bodyLen - 4);
            PcapNgInterface iface = !interfaces.isEmpty() ? interfaces[0] : PcapNgInterface{};
            RawPacket pkt;
            pkt.tsUsec = lastTsUsec; // SPB carries no timestamp
            pkt.linkType = iface.linkType;
            pkt.frame = QByteArray(reinterpret_cast<const char *>(body + 4), capLen);
            out->append(pkt);
        }
        pos += totalLen;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Link / network / transport decapsulation
// ---------------------------------------------------------------------------

struct ParsedTcp {
    QByteArray srcAddr, dstAddr; // 4 (IPv4) or 16 (IPv6) bytes
    quint16 srcPort = 0, dstPort = 0;
    quint32 seq = 0;
    bool syn = false, ack = false, fin = false;
    QByteArray payload;
};

// Returns the offset of the IP header within the frame and sets *ipVersion,
// or -1 if the frame does not carry IP.
qsizetype decapLink(const RawPacket &pkt, int *ipVersion) {
    const uchar *d = reinterpret_cast<const uchar *>(pkt.frame.constData());
    const qsizetype size = pkt.frame.size();

    auto versionFromEtherType = [](quint16 et) -> int {
        if (et == 0x0800) return 4;
        if (et == 0x86DD) return 6;
        return 0;
    };

    switch (pkt.linkType) {
    case LINKTYPE_ETHERNET: {
        qsizetype off = 14;
        if (size < off) return -1;
        quint16 etherType = readBE16(d + 12);
        // Skip 802.1Q / 802.1ad VLAN tags
        while ((etherType == 0x8100 || etherType == 0x88A8 || etherType == 0x9100)
               && size >= off + 4) {
            etherType = readBE16(d + off + 2);
            off += 4;
        }
        int v = versionFromEtherType(etherType);
        if (!v) return -1;
        *ipVersion = v;
        return off;
    }
    case LINKTYPE_LINUX_SLL: {
        if (size < 16) return -1;
        int v = versionFromEtherType(readBE16(d + 14));
        if (!v) return -1;
        *ipVersion = v;
        return 16;
    }
    case LINKTYPE_LINUX_SLL2: {
        if (size < 20) return -1;
        int v = versionFromEtherType(readBE16(d));
        if (!v) return -1;
        *ipVersion = v;
        return 20;
    }
    case LINKTYPE_NULL:
    case LINKTYPE_LOOP: {
        if (size < 4) return -1;
        // The 4-byte address family is written in the capture host's byte
        // order for NULL and big-endian for LOOP; accept either.
        quint32 famLE = readU32(d, false);
        quint32 famBE = readU32(d, true);
        auto familyToVersion = [](quint32 fam) -> int {
            if (fam == 2) return 4;                            // AF_INET
            if (fam == 24 || fam == 28 || fam == 30) return 6; // AF_INET6 variants
            return 0;
        };
        int v = familyToVersion(famLE);
        if (!v) v = familyToVersion(famBE);
        if (!v) return -1;
        *ipVersion = v;
        return 4;
    }
    case LINKTYPE_RAW: {
        if (size < 1) return -1;
        int v = d[0] >> 4;
        if (v != 4 && v != 6) return -1;
        *ipVersion = v;
        return 0;
    }
    default:
        return -1;
    }
}

bool parseTcpPacket(const RawPacket &pkt, ParsedTcp *out) {
    int ipVersion = 0;
    qsizetype ipOff = decapLink(pkt, &ipVersion);
    if (ipOff < 0) return false;

    const uchar *d = reinterpret_cast<const uchar *>(pkt.frame.constData());
    const qsizetype size = pkt.frame.size();
    qsizetype tcpOff = 0;
    qsizetype ipPayloadEnd = 0;

    if (ipVersion == 4) {
        if (size < ipOff + 20) return false;
        const uchar *ip = d + ipOff;
        if ((ip[0] >> 4) != 4) return false;
        const qsizetype ihl = (ip[0] & 0x0F) * 4;
        if (ihl < 20 || size < ipOff + ihl) return false;
        // Skip fragmented packets: MF set or fragment offset non-zero
        quint16 fragField = readBE16(ip + 6);
        if ((fragField & 0x2000) || (fragField & 0x1FFF)) return false;
        if (ip[9] != 6) return false; // not TCP
        qsizetype totalLen = readBE16(ip + 2);
        // Bound by the IP total length to trim Ethernet trailer padding
        ipPayloadEnd = qMin<qsizetype>(size, ipOff + totalLen);
        tcpOff = ipOff + ihl;
        out->srcAddr = QByteArray(reinterpret_cast<const char *>(ip + 12), 4);
        out->dstAddr = QByteArray(reinterpret_cast<const char *>(ip + 16), 4);
    } else {
        if (size < ipOff + 40) return false;
        const uchar *ip = d + ipOff;
        if ((ip[0] >> 4) != 6) return false;
        qsizetype payloadLen = readBE16(ip + 4);
        quint8 nextHeader = ip[6];
        out->srcAddr = QByteArray(reinterpret_cast<const char *>(ip + 8), 16);
        out->dstAddr = QByteArray(reinterpret_cast<const char *>(ip + 24), 16);
        qsizetype hdrOff = ipOff + 40;
        ipPayloadEnd = qMin<qsizetype>(size, hdrOff + payloadLen);
        // Walk extension headers until TCP
        while (nextHeader != 6) {
            if (nextHeader == 0 || nextHeader == 43 || nextHeader == 60) {
                if (ipPayloadEnd < hdrOff + 8) return false;
                quint8 next = d[hdrOff];
                qsizetype extLen = (d[hdrOff + 1] + 1) * 8;
                nextHeader = next;
                hdrOff += extLen;
            } else if (nextHeader == 44) { // fragment header
                if (ipPayloadEnd < hdrOff + 8) return false;
                quint16 fragField = readBE16(d + hdrOff + 2);
                if (fragField & 0xFFF9) return false; // offset != 0 or MF set
                nextHeader = d[hdrOff];
                hdrOff += 8;
            } else {
                return false; // unsupported header chain
            }
        }
        tcpOff = hdrOff;
    }

    if (ipPayloadEnd < tcpOff + 20) return false;
    const uchar *tcp = d + tcpOff;
    const qsizetype dataOff = (tcp[12] >> 4) * 4;
    if (dataOff < 20 || ipPayloadEnd < tcpOff + dataOff) return false;
    out->srcPort = readBE16(tcp);
    out->dstPort = readBE16(tcp + 2);
    out->seq = readBE32(tcp + 4);
    quint8 flags = tcp[13];
    out->syn = flags & 0x02;
    out->ack = flags & 0x10;
    out->fin = flags & 0x01;
    out->payload = pkt.frame.mid(tcpOff + dataOff, ipPayloadEnd - tcpOff - dataOff);
    return true;
}

// ---------------------------------------------------------------------------
// TCP flow tracking and reassembly
// ---------------------------------------------------------------------------

struct Endpoint {
    QByteArray addr;
    quint16 port = 0;

    bool operator==(const Endpoint &o) const { return port == o.port && addr == o.addr; }
    bool operator<(const Endpoint &o) const {
        if (addr != o.addr) return addr < o.addr;
        return port < o.port;
    }
    QString toString() const {
        QHostAddress ha;
        if (addr.size() == 4) {
            ha.setAddress(readBE32(reinterpret_cast<const uchar *>(addr.constData())));
        } else if (addr.size() == 16) {
            ha.setAddress(reinterpret_cast<const quint8 *>(addr.constData()));
        }
        QString s = ha.toString();
        return addr.size() == 16 ? QStringLiteral("[%1]:%2").arg(s).arg(port)
                                 : QStringLiteral("%1:%2").arg(s).arg(port);
    }
};

struct TcpSegment {
    qint64 tsUsec = 0;
    quint32 seq = 0;
    QByteArray payload;
};

struct FlowDirection {
    bool sawSyn = false;
    bool sawSynAck = false;
    bool isnKnown = false;
    quint32 isn = 0; // sequence number of the SYN
    qint64 payloadBytes = 0;
    QVector<TcpSegment> segments;
};

struct Flow {
    Endpoint a, b;       // a < b (canonical order)
    FlowDirection ab, ba; // a→b and b→a
    qint64 firstTsUsec = 0;
};

// A contiguous run of reassembled stream bytes with the timestamp of the
// packet that carried them.
struct StreamChunk {
    qint64 tsUsec = 0;
    QByteArray data;
};

QVector<StreamChunk> reassembleDirection(const FlowDirection &dir) {
    QVector<StreamChunk> chunks;
    if (dir.segments.isEmpty()) return chunks;

    // Establish the stream base: first byte after the SYN when we saw the
    // handshake, otherwise the smallest sequence number relative to the
    // earliest segment (signed 32-bit distance handles wraparound).
    quint32 base;
    if (dir.isnKnown) {
        base = dir.isn + 1;
    } else {
        base = dir.segments.first().seq;
        for (const TcpSegment &s : dir.segments) {
            if (qint32(s.seq - base) < 0) base = s.seq;
        }
    }

    QVector<const TcpSegment *> ordered;
    ordered.reserve(dir.segments.size());
    for (const TcpSegment &s : dir.segments) ordered.append(&s);
    std::stable_sort(ordered.begin(), ordered.end(),
        [base](const TcpSegment *l, const TcpSegment *r) {
            return qint32(l->seq - base) < qint32(r->seq - base);
        });

    qint64 nextExpected = 0;
    for (const TcpSegment *s : ordered) {
        qint64 relStart = qint64(qint32(s->seq - base));
        if (relStart > nextExpected) {
            // Hole in the capture: bytes were lost. The telnet stream can't
            // be parsed past the gap, so stop here.
            break;
        }
        qint64 skip = nextExpected - relStart; // retransmitted/overlapping prefix
        if (skip >= s->payload.size()) continue;
        StreamChunk chunk;
        chunk.tsUsec = s->tsUsec;
        chunk.data = s->payload.mid(skip);
        nextExpected += chunk.data.size();
        chunks.append(chunk);
    }
    return chunks;
}

// ---------------------------------------------------------------------------
// Passive telnet stream extraction
// ---------------------------------------------------------------------------

// Strips the telnet layer from a reassembled host→client stream and splits
// the application data into records at IAC EOR boundaries. Mirrors the
// parsing semantics of TN5250Client::processTelnetData, minus the replies.
QVector<ReplayRecord> extractRecords(const QVector<StreamChunk> &chunks) {
    QByteArray stream;
    QVector<qsizetype> chunkEnds;
    QVector<qint64> chunkTs;
    for (const StreamChunk &c : chunks) {
        stream.append(c.data);
        chunkEnds.append(stream.size());
        chunkTs.append(c.tsUsec);
    }

    int tsIdx = 0;
    auto timestampAt = [&](qsizetype offset) -> qint64 {
        while (tsIdx < chunkEnds.size() - 1 && offset >= chunkEnds[tsIdx]) tsIdx++;
        return chunkTs.isEmpty() ? 0 : chunkTs[tsIdx];
    };

    QVector<ReplayRecord> records;
    QByteArray current;
    qint64 currentTs = 0;
    bool inSubnegotiation = false;
    const uchar IAC = static_cast<uchar>(TelnetCommand::IAC);

    qsizetype i = 0;
    const qsizetype size = stream.size();
    while (i < size) {
        uchar byte = static_cast<uchar>(stream[i]);

        if (byte == IAC) {
            if (i + 1 >= size) break; // truncated escape at capture end
            uchar next = static_cast<uchar>(stream[i + 1]);

            if (next == IAC) {
                // Literal 0xFF. Inside a subnegotiation it belongs to the SB
                // payload (which replay discards); outside it is app data.
                if (!inSubnegotiation) {
                    current.append(char(0xFF));
                    currentTs = timestampAt(i);
                }
                i += 2;
                continue;
            }

            if (telnet::isStandaloneTelnetCommand(next)) {
                if (next == static_cast<uchar>(TelnetCommand::SE)) {
                    inSubnegotiation = false;
                } else if (next == static_cast<uchar>(TelnetCommand::EOR)) {
                    if (!current.isEmpty()) {
                        records.append({timestampAt(i), current});
                        current.clear();
                    }
                }
                i += 2;
                continue;
            }

            // WILL / WONT / DO / DONT / SB take an option byte
            if (i + 2 >= size) break;
            if (next == static_cast<uchar>(TelnetCommand::SB)) {
                inSubnegotiation = true;
            }
            i += 3;
        } else if (inSubnegotiation) {
            i++;
        } else {
            current.append(char(byte));
            currentTs = timestampAt(i);
            i++;
        }
    }

    // Capture ended mid-record (no final IAC EOR): keep what we have, the
    // decoder parses by GDS length prefix and tolerates a short tail.
    if (!current.isEmpty()) {
        records.append({currentTs, current});
    }
    return records;
}

// A direction looks like TN5250 when its stream opens with telnet
// negotiation (IAC) or, for captures started mid-session, directly with a
// GDS record (record type 0x12A0 at offset 2 of the length-prefixed record).
bool looksLikeTn5250(const FlowDirection &dir) {
    for (const TcpSegment &s : dir.segments) {
        if (s.payload.isEmpty()) continue;
        if (static_cast<uchar>(s.payload[0]) == static_cast<uchar>(TelnetCommand::IAC)) {
            return true;
        }
        return s.payload.size() >= 4
            && static_cast<uchar>(s.payload[2]) == 0x12
            && static_cast<uchar>(s.payload[3]) == 0xA0;
    }
    return false;
}

bool isTelnetServerPort(quint16 port) {
    return port == 23 || port == 992;
}

} // namespace

// ---------------------------------------------------------------------------
// PcapReplayLoader
// ---------------------------------------------------------------------------

bool PcapReplayLoader::loadFile(const QString &path, ReplaySession *out, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open file: %1").arg(path);
        return false;
    }
    return loadData(file.readAll(), out, error);
}

bool PcapReplayLoader::loadData(const QByteArray &data, ReplaySession *out, QString *error) {
    QVector<RawPacket> packets;
    if (isClassicPcap(data)) {
        if (!parseClassicPcap(data, &packets, error)) return false;
    } else if (isPcapNg(data)) {
        if (!parsePcapNg(data, &packets, error)) return false;
    } else {
        if (error) *error = QStringLiteral("Not a pcap or pcapng capture file");
        return false;
    }

    // Group TCP segments into bidirectional flows
    QVector<Flow> flows;
    QHash<QByteArray, int> flowIndex;
    for (const RawPacket &pkt : packets) {
        ParsedTcp tcp;
        if (!parseTcpPacket(pkt, &tcp)) continue;

        Endpoint src{tcp.srcAddr, tcp.srcPort};
        Endpoint dst{tcp.dstAddr, tcp.dstPort};
        bool srcIsA = src < dst;
        const Endpoint &a = srcIsA ? src : dst;
        const Endpoint &b = srcIsA ? dst : src;

        QByteArray key = a.addr;
        key.append(reinterpret_cast<const char *>(&a.port), 2);
        key.append(b.addr);
        key.append(reinterpret_cast<const char *>(&b.port), 2);

        int idx = flowIndex.value(key, -1);
        if (idx < 0) {
            idx = flows.size();
            flowIndex.insert(key, idx);
            Flow flow;
            flow.a = a;
            flow.b = b;
            flow.firstTsUsec = pkt.tsUsec;
            flows.append(flow);
        }
        FlowDirection &dir = srcIsA ? flows[idx].ab : flows[idx].ba;

        if (tcp.syn) {
            dir.sawSyn = true;
            dir.sawSynAck = tcp.ack;
            dir.isnKnown = true;
            dir.isn = tcp.seq;
        }
        if (!tcp.payload.isEmpty()) {
            TcpSegment seg;
            seg.tsUsec = pkt.tsUsec;
            seg.seq = tcp.seq;
            seg.payload = tcp.payload;
            dir.payloadBytes += tcp.payload.size();
            dir.segments.append(seg);
        }
    }

    if (flows.isEmpty()) {
        if (error) *error = QStringLiteral("No TCP traffic found in capture");
        return false;
    }

    // Select the telnet/TN5250 flow: a flow where at least one direction's
    // stream opens with telnet negotiation or GDS records. Among candidates
    // take the one with the most payload. A capture without any such flow is
    // rejected — replaying arbitrary TCP bytes through the 5250 decoder
    // would only render garbage.
    Flow *selected = nullptr;
    qint64 selectedBytes = -1;
    for (Flow &flow : flows) {
        if (!looksLikeTn5250(flow.ab) && !looksLikeTn5250(flow.ba)) continue;
        qint64 bytes = flow.ab.payloadBytes + flow.ba.payloadBytes;
        if (bytes > selectedBytes) {
            selected = &flow;
            selectedBytes = bytes;
        }
    }
    if (!selected) {
        if (error) *error = QStringLiteral("No telnet/TN5250 flow found in capture");
        return false;
    }

    // Identify the server→client direction:
    // 1. The direction that sent SYN+ACK is from the server.
    // 2. A plain SYN identifies the client side.
    // 3. A well-known telnet port (23/992) identifies the server endpoint.
    // 4. Fall back to the direction with more payload (the host sends far
    //    more data than the terminal).
    bool serverIsA;
    if (selected->ab.sawSynAck || selected->ba.sawSynAck) {
        serverIsA = selected->ab.sawSynAck;
    } else if (selected->ab.sawSyn || selected->ba.sawSyn) {
        serverIsA = selected->ba.sawSyn;
    } else if (isTelnetServerPort(selected->a.port) != isTelnetServerPort(selected->b.port)) {
        serverIsA = isTelnetServerPort(selected->a.port);
    } else {
        serverIsA = selected->ab.payloadBytes >= selected->ba.payloadBytes;
    }

    const FlowDirection &hostToClient = serverIsA ? selected->ab : selected->ba;
    out->serverEndpoint = (serverIsA ? selected->a : selected->b).toString();
    out->clientEndpoint = (serverIsA ? selected->b : selected->a).toString();
    out->records = extractRecords(reassembleDirection(hostToClient));

    if (out->records.isEmpty()) {
        if (error) *error = QStringLiteral("No TN5250 session data found in capture");
        return false;
    }
    return true;
}

} // namespace core::pcap
