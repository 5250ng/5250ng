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

#include "network/hostserver/host_constants.h"
#include "network/hostserver/host_data_stream.h"
#include <QByteArray>
#include <QString>
#include <QTest>

using namespace hostserver;

class TestHostDataStream : public QObject {
    Q_OBJECT

  private slots:
    void testReadU16() {
        QByteArray data;
        data.append(static_cast<char>(0x12));
        data.append(static_cast<char>(0xA0));
        QCOMPARE(HostDataStream::readU16(data, 0), static_cast<uint16_t>(0x12A0));
    }

    void testReadU32() {
        QByteArray data;
        data.append(static_cast<char>(0x00));
        data.append(static_cast<char>(0x00));
        data.append(static_cast<char>(0x00));
        data.append(static_cast<char>(0x34));
        QCOMPARE(HostDataStream::readU32(data, 0), static_cast<uint32_t>(0x34));
    }

    void testReadU64() {
        QByteArray data(8, '\0');
        data[6] = static_cast<char>(0x01);
        data[7] = static_cast<char>(0x00);
        QCOMPARE(HostDataStream::readU64(data, 0), static_cast<uint64_t>(256));
    }

    void testWriteU16() {
        QByteArray buf;
        HostDataStream::writeU16(buf, 0xE009);
        QCOMPARE(buf.size(), 2);
        QCOMPARE(static_cast<uint8_t>(buf[0]), static_cast<uint8_t>(0xE0));
        QCOMPARE(static_cast<uint8_t>(buf[1]), static_cast<uint8_t>(0x09));
    }

    void testWriteU32() {
        QByteArray buf;
        HostDataStream::writeU32(buf, 0x00000034);
        QCOMPARE(buf.size(), 4);
        QCOMPARE(HostDataStream::readU32(buf, 0), static_cast<uint32_t>(0x34));
    }

    void testWriteU64() {
        QByteArray buf;
        HostDataStream::writeU64(buf, 0x0000000100000000ULL);
        QCOMPARE(buf.size(), 8);
        QCOMPARE(HostDataStream::readU64(buf, 0), static_cast<uint64_t>(0x0000000100000000ULL));
    }

    void testBuildPacket() {
        QByteArray payload;
        payload.append(static_cast<char>(0xAA));
        payload.append(static_cast<char>(0xBB));

        QByteArray pkt = HostDataStream::buildPacket(
            0x00, 0x00, ServerID::Signon,
            0x00000001, 0x0000, 0x7003, payload);

        QCOMPARE(pkt.size(), 22); // 20 header + 2 payload
        QCOMPARE(HostDataStream::readU32(pkt, 0), static_cast<uint32_t>(22)); // length
        QCOMPARE(static_cast<uint8_t>(pkt[4]), static_cast<uint8_t>(0x00)); // client attr
        QCOMPARE(HostDataStream::readU16(pkt, 6), static_cast<uint16_t>(0xE009)); // server ID
        QCOMPARE(HostDataStream::readU32(pkt, 12), static_cast<uint32_t>(1)); // correlation
        QCOMPARE(HostDataStream::readU16(pkt, 18), static_cast<uint16_t>(0x7003)); // reqrep
        QCOMPARE(static_cast<uint8_t>(pkt[20]), static_cast<uint8_t>(0xAA));
        QCOMPARE(static_cast<uint8_t>(pkt[21]), static_cast<uint8_t>(0xBB));
    }

    void testParseHeader() {
        QByteArray pkt = HostDataStream::buildPacket(
            0x01, 0x02, ServerID::File,
            0x00000005, 8, 0x7001, QByteArray(8, '\0'));

        uint32_t length;
        uint8_t clientAttr, serverAttr;
        uint16_t serverId, templateLength, reqRepId;
        uint32_t csInstance, correlation;

        bool ok = HostDataStream::parseHeader(pkt, length, clientAttr, serverAttr,
                                               serverId, csInstance, correlation,
                                               templateLength, reqRepId);
        QVERIFY(ok);
        QCOMPARE(length, static_cast<uint32_t>(28));
        QCOMPARE(clientAttr, static_cast<uint8_t>(0x01));
        QCOMPARE(serverAttr, static_cast<uint8_t>(0x02));
        QCOMPARE(serverId, static_cast<uint16_t>(0xE002));
        QCOMPARE(correlation, static_cast<uint32_t>(5));
        QCOMPARE(templateLength, static_cast<uint16_t>(8));
        QCOMPARE(reqRepId, static_cast<uint16_t>(0x7001));
    }

    void testParseHeaderTooShort() {
        QByteArray data(10, '\0');
        uint32_t l; uint8_t ca, sa; uint16_t si, tl, rr; uint32_t cs, co;
        QVERIFY(!HostDataStream::parseHeader(data, l, ca, sa, si, cs, co, tl, rr));
    }

    void testBuildLLCP() {
        QByteArray data;
        data.append(static_cast<char>(0x01));
        data.append(static_cast<char>(0x02));

        QByteArray field = HostDataStream::buildLLCP(0x1103, data);
        // LL = 2 + 6 = 8
        QCOMPARE(field.size(), 8);
        QCOMPARE(HostDataStream::readU32(field, 0), static_cast<uint32_t>(8)); // LL
        QCOMPARE(HostDataStream::readU16(field, 4), static_cast<uint16_t>(0x1103)); // CP
        QCOMPARE(static_cast<uint8_t>(field[6]), static_cast<uint8_t>(0x01));
        QCOMPARE(static_cast<uint8_t>(field[7]), static_cast<uint8_t>(0x02));
    }

    void testFindCodePoint() {
        QByteArray data;
        // First field: CP 0x1101, value = 0x00000001
        QByteArray v1;
        HostDataStream::writeU32(v1, 1);
        data.append(HostDataStream::buildLLCP(0x1101, v1));

        // Second field: CP 0x1103, value = 8 bytes of 0xAA
        QByteArray v2(8, static_cast<char>(0xAA));
        data.append(HostDataStream::buildLLCP(0x1103, v2));

        // Find first
        QByteArray found1 = HostDataStream::findCodePoint(data, 0, 0x1101);
        QCOMPARE(found1.size(), 4);
        QCOMPARE(HostDataStream::readU32(found1, 0), static_cast<uint32_t>(1));

        // Find second
        QByteArray found2 = HostDataStream::findCodePoint(data, 0, 0x1103);
        QCOMPARE(found2.size(), 8);
        QCOMPARE(static_cast<uint8_t>(found2[0]), static_cast<uint8_t>(0xAA));

        // Not found
        QByteArray notFound = HostDataStream::findCodePoint(data, 0, 0x9999);
        QVERIFY(notFound.isEmpty());
    }

    void testFindCodePointRejectsHighBitLength() {
        // A malicious LLCP length field with the high bit set would cast
        // to a negative int before the pre-fix bounds check, bypass it,
        // and cause parsing to wrap the offset via signed overflow.
        // The parser must treat such a length as out of range and stop.
        QByteArray data;
        HostDataStream::writeU32(data, 0x80000000u); // LL with high bit set
        HostDataStream::writeU16(data, 0x1101);      // CP
        data.append(QByteArray(16, '\0'));           // some filler bytes

        QByteArray found = HostDataStream::findCodePoint(data, 0, 0x1101);
        QVERIFY(found.isEmpty());
    }

    void testFindCodePointRejectsLengthBeyondBuffer() {
        // Length that is positive as signed int but exceeds the remaining
        // buffer. The parser must stop without reading past the end.
        QByteArray data;
        HostDataStream::writeU32(data, 0x00010000u); // LL = 65536
        HostDataStream::writeU16(data, 0x1101);      // CP
        data.append(QByteArray(16, '\0'));

        QByteArray found = HostDataStream::findCodePoint(data, 0, 0x1101);
        QVERIFY(found.isEmpty());
    }

    void testUtf16BERoundTrip() {
        QString original = QStringLiteral("/home/MYUSER/test.txt");
        QByteArray encoded = HostDataStream::toUtf16BE(original);
        QCOMPARE(encoded.size(), original.size() * 2);
        QString decoded = HostDataStream::fromUtf16BE(encoded);
        QCOMPARE(decoded, original);
    }

    void testUtf16BEEmpty() {
        QByteArray encoded = HostDataStream::toUtf16BE(QString());
        QVERIFY(encoded.isEmpty());
        QString decoded = HostDataStream::fromUtf16BE(QByteArray());
        QVERIFY(decoded.isEmpty());
    }

    void testEncodeUserId() {
        QByteArray encoded = HostDataStream::encodeUserId(QStringLiteral("MYUSER"));
        QCOMPARE(encoded.size(), 10);
        // Should be uppercase EBCDIC, padded with 0x40
        QCOMPARE(static_cast<uint8_t>(encoded[6]), static_cast<uint8_t>(0x40)); // padding
        QCOMPARE(static_cast<uint8_t>(encoded[9]), static_cast<uint8_t>(0x40));
    }

    void testEncodeUserIdTruncation() {
        QByteArray encoded = HostDataStream::encodeUserId(
            QStringLiteral("VERYLONGUSERNAME"));
        QCOMPARE(encoded.size(), 10); // Truncated to 10
    }

    void testServerIDs() {
        QCOMPARE(static_cast<uint16_t>(ServerID::Signon), static_cast<uint16_t>(0xE009));
        QCOMPARE(static_cast<uint16_t>(ServerID::File), static_cast<uint16_t>(0xE002));
        QCOMPARE(static_cast<uint16_t>(ServerID::Database), static_cast<uint16_t>(0xE004));
    }

    void testIFSReturnCodeStrings() {
        QCOMPARE(ifs_rc::toString(ifs_rc::SUCCESS), "Success");
        QCOMPARE(ifs_rc::toString(ifs_rc::FILE_NOT_FOUND), "File not found");
        QCOMPARE(ifs_rc::toString(ifs_rc::ACCESS_DENIED), "Access denied");
        QCOMPARE(ifs_rc::toString(ifs_rc::NO_MORE_DATA), "No more data (EOF)");
        QCOMPARE(ifs_rc::toString(999), "Unknown error");
    }

    void testDefaultPorts() {
        QCOMPARE(ports::SIGNON, static_cast<uint16_t>(8476));
        QCOMPARE(ports::FILE, static_cast<uint16_t>(8473));
        QCOMPARE(ports::DATABASE, static_cast<uint16_t>(8471));
        QCOMPARE(ports::SERVICE_MAPPER, static_cast<uint16_t>(449));
    }
};

QTEST_MAIN(TestHostDataStream)
#include "test_host_data_stream.moc"
