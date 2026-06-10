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

#include "core/pcap_replay.h"

#include <QtTest/QtTest>

using core::pcap::PcapReplayLoader;
using core::pcap::ReplaySession;

namespace {

// ---------------------------------------------------------------------------
// Synthetic packet builders
// ---------------------------------------------------------------------------

void appendBE16(QByteArray &b, quint16 v) {
    b.append(char(v >> 8));
    b.append(char(v & 0xFF));
}

void appendBE32(QByteArray &b, quint32 v) {
    b.append(char(v >> 24));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
}

void appendLE32(QByteArray &b, quint32 v) {
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char(v >> 24));
}

struct TcpParams {
    quint32 srcIp = 0;
    quint32 dstIp = 0;
    quint16 srcPort = 0;
    quint16 dstPort = 0;
    quint32 seq = 0;
    quint32 ack = 0;
    quint8 flags = 0x10; // ACK
    QByteArray payload;
    int etherPadding = 0; // trailer bytes after the IP datagram
    bool vlanTag = false;
};

constexpr quint8 TCP_SYN = 0x02;
constexpr quint8 TCP_ACK = 0x10;

// Ethernet + IPv4 + TCP frame
QByteArray buildFrame(const TcpParams &p) {
    QByteArray tcp;
    appendBE16(tcp, p.srcPort);
    appendBE16(tcp, p.dstPort);
    appendBE32(tcp, p.seq);
    appendBE32(tcp, p.ack);
    tcp.append(char(5 << 4)); // data offset: 5 words, no options
    tcp.append(char(p.flags));
    appendBE16(tcp, 8192); // window
    appendBE16(tcp, 0);    // checksum (unchecked)
    appendBE16(tcp, 0);    // urgent pointer
    tcp.append(p.payload);

    QByteArray ip;
    ip.append(char(0x45)); // v4, ihl=5
    ip.append(char(0x00)); // DSCP
    appendBE16(ip, quint16(20 + tcp.size()));
    appendBE16(ip, 0x1234); // identification
    appendBE16(ip, 0x4000); // DF, no fragment offset
    ip.append(char(64));    // TTL
    ip.append(char(6));     // TCP
    appendBE16(ip, 0);      // checksum (unchecked)
    appendBE32(ip, p.srcIp);
    appendBE32(ip, p.dstIp);
    ip.append(tcp);

    QByteArray frame;
    frame.append(QByteArray(6, char(0x02))); // dst MAC
    frame.append(QByteArray(6, char(0x04))); // src MAC
    if (p.vlanTag) {
        appendBE16(frame, 0x8100);
        appendBE16(frame, 42); // VLAN id
    }
    appendBE16(frame, 0x0800);
    frame.append(ip);
    frame.append(QByteArray(p.etherPadding, char(0xEE)));
    return frame;
}

struct CapturePacket {
    qint64 tsUsec = 0;
    QByteArray frame;
};

// Classic pcap container, little-endian, microsecond resolution, Ethernet
QByteArray buildClassicPcap(const QVector<CapturePacket> &packets,
                            bool nanosecond = false) {
    QByteArray out;
    appendLE32(out, nanosecond ? 0xA1B23C4D : 0xA1B2C3D4);
    out.append(char(2)); out.append(char(0)); // version major
    out.append(char(4)); out.append(char(0)); // version minor
    appendLE32(out, 0);     // thiszone
    appendLE32(out, 0);     // sigfigs
    appendLE32(out, 65535); // snaplen
    appendLE32(out, 1);     // LINKTYPE_ETHERNET
    for (const CapturePacket &p : packets) {
        appendLE32(out, quint32(p.tsUsec / 1000000));
        quint32 frac = quint32(p.tsUsec % 1000000);
        appendLE32(out, nanosecond ? frac * 1000 : frac);
        appendLE32(out, quint32(p.frame.size()));
        appendLE32(out, quint32(p.frame.size()));
        out.append(p.frame);
    }
    return out;
}

// Classic pcap container, big-endian byte order
QByteArray buildBigEndianPcap(const QVector<CapturePacket> &packets) {
    QByteArray out;
    appendBE32(out, 0xA1B2C3D4);
    out.append(char(0)); out.append(char(2));
    out.append(char(0)); out.append(char(4));
    appendBE32(out, 0);
    appendBE32(out, 0);
    appendBE32(out, 65535);
    appendBE32(out, 1);
    for (const CapturePacket &p : packets) {
        appendBE32(out, quint32(p.tsUsec / 1000000));
        appendBE32(out, quint32(p.tsUsec % 1000000));
        appendBE32(out, quint32(p.frame.size()));
        appendBE32(out, quint32(p.frame.size()));
        out.append(p.frame);
    }
    return out;
}

// pcapng container: SHB + one Ethernet IDB + one EPB per packet
QByteArray buildPcapNg(const QVector<CapturePacket> &packets) {
    QByteArray out;

    QByteArray shb;
    appendLE32(shb, 0x1A2B3C4D); // byte-order magic
    shb.append(char(1)); shb.append(char(0)); // major
    shb.append(char(0)); shb.append(char(0)); // minor
    shb.append(QByteArray(8, char(0xFF)));    // section length: unspecified
    appendLE32(out, 0x0A0D0D0A);
    appendLE32(out, quint32(shb.size() + 12));
    out.append(shb);
    appendLE32(out, quint32(shb.size() + 12));

    QByteArray idb;
    idb.append(char(1)); idb.append(char(0)); // LINKTYPE_ETHERNET
    idb.append(char(0)); idb.append(char(0)); // reserved
    appendLE32(idb, 65535);                   // snaplen
    appendLE32(out, 1);
    appendLE32(out, quint32(idb.size() + 12));
    out.append(idb);
    appendLE32(out, quint32(idb.size() + 12));

    for (const CapturePacket &p : packets) {
        QByteArray epb;
        appendLE32(epb, 0); // interface id
        quint64 ts = quint64(p.tsUsec);
        appendLE32(epb, quint32(ts >> 32));
        appendLE32(epb, quint32(ts & 0xFFFFFFFF));
        appendLE32(epb, quint32(p.frame.size()));
        appendLE32(epb, quint32(p.frame.size()));
        epb.append(p.frame);
        while (epb.size() % 4) epb.append(char(0));
        appendLE32(out, 6);
        appendLE32(out, quint32(epb.size() + 12));
        out.append(epb);
        appendLE32(out, quint32(epb.size() + 12));
    }
    return out;
}

// ---------------------------------------------------------------------------
// TN5250 stream fragments
// ---------------------------------------------------------------------------

const char IAC = char(0xFF);
const char DO = char(0xFD);
const char WILL = char(0xFB);
const char SB = char(0xFA);
const char SE = char(0xF0);
const char EOR_CMD = char(0xEF);
const char OPT_TERMINAL_TYPE = char(0x18);
const char OPT_EOR = char(0x19);

// A minimal GDS record: length prefix, type 0x12A0, header, payload
QByteArray gdsRecord(const QByteArray &payload) {
    QByteArray rec;
    appendBE16(rec, quint16(10 + payload.size()));
    rec.append(char(0x12));
    rec.append(char(0xA0));
    appendBE16(rec, 0);     // reserved
    rec.append(char(0x04)); // variable header length
    rec.append(char(0x00)); // flags
    rec.append(char(0x00));
    rec.append(char(0x03)); // opcode
    rec.append(payload);
    return rec;
}

QByteArray telnetNegotiation() {
    QByteArray b;
    b.append(IAC); b.append(DO); b.append(OPT_TERMINAL_TYPE);
    b.append(IAC); b.append(SB); b.append(OPT_TERMINAL_TYPE);
    b.append(char(0x01)); // SEND
    b.append(IAC); b.append(SE);
    b.append(IAC); b.append(DO); b.append(OPT_EOR);
    b.append(IAC); b.append(WILL); b.append(OPT_EOR);
    return b;
}

QByteArray withEor(const QByteArray &record) {
    QByteArray b = record;
    b.append(IAC);
    b.append(EOR_CMD);
    return b;
}

// IAC-escape record content the way a live host would
QByteArray iacEscaped(const QByteArray &data) {
    QByteArray b;
    for (char c : data) {
        b.append(c);
        if (c == IAC) b.append(IAC);
    }
    return b;
}

constexpr quint32 SERVER_IP = 0x0A000001; // 10.0.0.1
constexpr quint32 CLIENT_IP = 0x0A000002; // 10.0.0.2
constexpr quint16 SERVER_PORT = 23;
constexpr quint16 CLIENT_PORT = 51000;

// A full TN5250 conversation: handshake, negotiation, then the given
// host→client stream split into segments. Returns packets ready for a
// container builder.
struct ConversationBuilder {
    QVector<CapturePacket> packets;
    quint32 serverSeq = 1000;
    quint32 clientSeq = 5000;
    qint64 ts = 1700000000000000LL; // µs

    void handshake() {
        TcpParams syn;
        syn.srcIp = CLIENT_IP; syn.dstIp = SERVER_IP;
        syn.srcPort = CLIENT_PORT; syn.dstPort = SERVER_PORT;
        syn.seq = clientSeq; syn.flags = TCP_SYN;
        packets.append({ts, buildFrame(syn)});
        ts += 1000;

        TcpParams synAck;
        synAck.srcIp = SERVER_IP; synAck.dstIp = CLIENT_IP;
        synAck.srcPort = SERVER_PORT; synAck.dstPort = CLIENT_PORT;
        synAck.seq = serverSeq; synAck.ack = clientSeq + 1;
        synAck.flags = TCP_SYN | TCP_ACK;
        packets.append({ts, buildFrame(synAck)});
        ts += 1000;
        serverSeq++;
        clientSeq++;
    }

    void serverSends(const QByteArray &data, qint64 advanceUsec = 1000,
                     int etherPadding = 0) {
        TcpParams p;
        p.srcIp = SERVER_IP; p.dstIp = CLIENT_IP;
        p.srcPort = SERVER_PORT; p.dstPort = CLIENT_PORT;
        p.seq = serverSeq; p.payload = data;
        p.etherPadding = etherPadding;
        packets.append({ts, buildFrame(p)});
        serverSeq += data.size();
        ts += advanceUsec;
    }

    void clientSends(const QByteArray &data) {
        TcpParams p;
        p.srcIp = CLIENT_IP; p.dstIp = SERVER_IP;
        p.srcPort = CLIENT_PORT; p.dstPort = SERVER_PORT;
        p.seq = clientSeq; p.payload = data;
        packets.append({ts, buildFrame(p)});
        clientSeq += data.size();
        ts += 1000;
    }
};

} // namespace

// ---------------------------------------------------------------------------

class TestPcapReplay : public QObject {
    Q_OBJECT

  private slots:

    void testClassicPcapBasicSession() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        conv.clientSends(QByteArray(1, IAC) + QByteArray(1, WILL)
                         + QByteArray(1, OPT_TERMINAL_TYPE));
        QByteArray rec1 = gdsRecord(QByteArray("\x40\x11", 2) + QByteArray("SCREEN-ONE"));
        QByteArray rec2 = gdsRecord(QByteArray("\x40\x11", 2) + QByteArray("SCREEN-TWO"));
        conv.serverSends(withEor(rec1));
        conv.serverSends(withEor(rec2));

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 2);
        QCOMPARE(session.records[0].data, rec1);
        QCOMPARE(session.records[1].data, rec2);
        QCOMPARE(session.serverEndpoint, QStringLiteral("10.0.0.1:23"));
        QCOMPARE(session.clientEndpoint, QStringLiteral("10.0.0.2:51000"));
        QVERIFY(session.records[1].timestampUsec > session.records[0].timestampUsec);
    }

    void testPcapNgSession() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("NG-PAYLOAD"));
        conv.serverSends(withEor(rec));

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildPcapNg(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testNanosecondPcap() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("NSEC"));
        conv.serverSends(withEor(rec));

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets, true),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
        QCOMPARE(session.records[0].timestampUsec % 1000000,
                 conv.packets.last().tsUsec % 1000000);
    }

    void testBigEndianPcap() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("BIGEND"));
        conv.serverSends(withEor(rec));

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildBigEndianPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testIacIacUnescapedInRecord() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("\x01") + QByteArray(2, IAC)
                                   + QByteArray("\x02"));
        // The record contains 0xFF bytes: the host doubles them on the wire
        conv.serverSends(iacEscaped(rec) + QByteArray(1, IAC)
                         + QByteArray(1, EOR_CMD));

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testSubnegotiationWithIacIacStripped() {
        ConversationBuilder conv;
        conv.handshake();
        // Subnegotiation payload contains an escaped 0xFF which must NOT
        // leak into the application stream
        QByteArray neg;
        neg.append(IAC); neg.append(SB); neg.append(OPT_TERMINAL_TYPE);
        neg.append(char(0x01));
        neg.append(IAC); neg.append(IAC); // literal 0xFF inside SB payload
        neg.append(char(0x02));
        neg.append(IAC); neg.append(SE);
        conv.serverSends(neg);
        QByteArray rec = gdsRecord(QByteArray("AFTER-SB"));
        conv.serverSends(withEor(rec));

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testRecordSplitAcrossSegments() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("SPLIT-ACROSS-SEGMENTS"));
        QByteArray wire = withEor(rec);
        conv.serverSends(wire.left(7));
        conv.serverSends(wire.mid(7));

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testRetransmissionDeduplicated() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("RETRANSMITTED"));
        QByteArray wire = withEor(rec);
        conv.serverSends(wire);
        // Full retransmission of the same segment (stale seq)
        TcpParams dup;
        dup.srcIp = SERVER_IP; dup.dstIp = CLIENT_IP;
        dup.srcPort = SERVER_PORT; dup.dstPort = CLIENT_PORT;
        dup.seq = conv.serverSeq - wire.size();
        dup.payload = wire;
        conv.packets.append({conv.ts, buildFrame(dup)});

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testOutOfOrderSegmentsReassembled() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("OUT-OF-ORDER-DATA"));
        QByteArray wire = withEor(rec);
        QByteArray part1 = wire.left(9);
        QByteArray part2 = wire.mid(9);
        quint32 seqBase = conv.serverSeq;
        // Append part2 first (higher seq), then part1
        TcpParams p2;
        p2.srcIp = SERVER_IP; p2.dstIp = CLIENT_IP;
        p2.srcPort = SERVER_PORT; p2.dstPort = CLIENT_PORT;
        p2.seq = seqBase + part1.size(); p2.payload = part2;
        conv.packets.append({conv.ts, buildFrame(p2)});
        TcpParams p1 = p2;
        p1.seq = seqBase; p1.payload = part1;
        conv.packets.append({conv.ts + 500, buildFrame(p1)});

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testEthernetTrailerPaddingTrimmed() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("PAD"));
        // Short frame padded by the NIC to the Ethernet minimum: the pad
        // bytes sit after the IP datagram and must not enter the stream
        conv.serverSends(withEor(rec), 1000, 18);

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testVlanTaggedFrames() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("VLAN"));
        QByteArray wire = withEor(rec);
        TcpParams p;
        p.srcIp = SERVER_IP; p.dstIp = CLIENT_IP;
        p.srcPort = SERVER_PORT; p.dstPort = CLIENT_PORT;
        p.seq = conv.serverSeq; p.payload = wire; p.vlanTag = true;
        conv.packets.append({conv.ts, buildFrame(p)});

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testTelnetFlowPreferredOverLargerNoiseFlow() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("REAL-SESSION"));
        conv.serverSends(withEor(rec));

        // A bigger non-telnet flow (e.g. an HTTP transfer in the same capture)
        TcpParams noise;
        noise.srcIp = 0x0A000003; noise.dstIp = 0x0A000004;
        noise.srcPort = 80; noise.dstPort = 40000;
        noise.seq = 1;
        noise.payload = QByteArray(4096, 'x');
        conv.packets.prepend({conv.ts - 50000, buildFrame(noise)});

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
        QCOMPARE(session.serverEndpoint, QStringLiteral("10.0.0.1:23"));
    }

    void testServerInferredFromPortWithoutHandshake() {
        // Capture started mid-session: no SYN packets at all
        ConversationBuilder conv;
        conv.serverSeq = 100000;
        conv.clientSeq = 200000;
        QByteArray rec = gdsRecord(QByteArray("MID-SESSION"));
        conv.serverSends(withEor(rec));
        conv.clientSends(QByteArray(1, IAC) + QByteArray(1, WILL)
                         + QByteArray(1, OPT_EOR));

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.serverEndpoint, QStringLiteral("10.0.0.1:23"));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testFinalRecordWithoutEorKept() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("CUT-OFF"));
        conv.serverSends(rec); // capture ends before IAC EOR arrives

        ReplaySession session;
        QString error;
        QVERIFY2(PcapReplayLoader::loadData(buildClassicPcap(conv.packets),
                                            &session, &error),
                 qPrintable(error));
        QCOMPARE(session.records.size(), 1);
        QCOMPARE(session.records[0].data, rec);
    }

    void testNotACaptureFile() {
        ReplaySession session;
        QString error;
        QVERIFY(!PcapReplayLoader::loadData(QByteArray("definitely not a pcap"),
                                            &session, &error));
        QVERIFY(!error.isEmpty());
    }

    void testEmptyCapture() {
        ReplaySession session;
        QString error;
        QVERIFY(!PcapReplayLoader::loadData(buildClassicPcap({}), &session, &error));
        QVERIFY(!error.isEmpty());
    }

    void testCaptureWithoutTelnetTraffic() {
        QVector<CapturePacket> packets;
        TcpParams p;
        p.srcIp = 0x0A000003; p.dstIp = 0x0A000004;
        p.srcPort = 80; p.dstPort = 40000;
        p.seq = 1;
        p.payload = QByteArray("HTTP/1.1 200 OK\r\n\r\n");
        packets.append({1000, buildFrame(p)});

        ReplaySession session;
        QString error;
        QVERIFY(!PcapReplayLoader::loadData(buildClassicPcap(packets),
                                            &session, &error));
        QVERIFY(!error.isEmpty());
    }

    void testTruncatedCaptureTailTolerated() {
        ConversationBuilder conv;
        conv.handshake();
        conv.serverSends(telnetNegotiation());
        QByteArray rec = gdsRecord(QByteArray("BEFORE-TRUNCATION"));
        conv.serverSends(withEor(rec));
        QByteArray capture = buildClassicPcap(conv.packets);
        // A record header announcing more bytes than remain in the file
        capture.chop(10);
        // The cut hits the last packet's frame, so only the negotiation
        // and the cut packet's prefix survive; the loader must not crash
        // and must still report a coherent result or a clean error.
        ReplaySession session;
        QString error;
        bool ok = PcapReplayLoader::loadData(capture, &session, &error);
        if (ok) {
            QVERIFY(!session.records.isEmpty());
        } else {
            QVERIFY(!error.isEmpty());
        }
    }
};

QTEST_MAIN(TestPcapReplay)
#include "test_pcap_replay.moc"
