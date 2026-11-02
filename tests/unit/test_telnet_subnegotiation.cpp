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

#include "network/tn5250_qt/client/client.h"
#include "network/tn5250_qt/telnet/commands.h"
#include "network/tn5250_qt/telnet/options.h"
#include <QSignalSpy>
#include <QtTest/QtTest>

using namespace tn5250::client;
using telnet::TelnetCommand;
using telnet::TelnetOption;

// Friend-declared in TN5250Client so it can poke the private telnet parser
// and the subnegotiation buffer without exposing them in the public API.
class TestTelnetSubnegotiation : public QObject {
    Q_OBJECT

  private slots:
    void testEscapedIacInsideSubnegotiationStaysInSbBuffer();
    void testEscapedIacOutsideSubnegotiationFlowsAsAppData();
    void testIbmrseedSeedWithEscapedControlBytes();
    void testIbmrseedPlainSeedStillParsed();
    void testUnescapedIacBeforeDataByteIsData();
    void testUnescapedIacInsideRecordIsData();
    void testRecordsDeliveredAtEor();
    void testGdsLengthRepairedAtEor();
    void testNopBetweenRecordsIsCommand();

  private:
    static QByteArray makeFrame(std::initializer_list<uint8_t> bytes) {
        QByteArray b;
        for (uint8_t x : bytes) b.append(static_cast<char>(x));
        return b;
    }
};

// Regression test for the IAC-IAC routing bug in
// TN5250Client::processTelnetData: an escaped 0xFF inside an SB payload
// must be appended to m_subnegotiationBuffer, not to the application data
// stream emitted via dataReceived().
void TestTelnetSubnegotiation::testEscapedIacInsideSubnegotiationStaysInSbBuffer() {
    TN5250Client client;
    client.m_handshakeComplete = true; // dataReceived() guarded on this flag

    QSignalSpy spy(&client, &TN5250Client::dataReceived);

    // Use NAWS as the SB option because handleSubnegotiation for NAWS only
    // logs and returns — it doesn't try to write a reply through the (null)
    // socket. The payload contains two IAC IAC pairs, each of which must
    // decode to a single 0xFF byte inside the SB buffer.
    const uint8_t IAC = static_cast<uint8_t>(TelnetCommand::IAC);
    const uint8_t SB = static_cast<uint8_t>(TelnetCommand::SB);
    const uint8_t SE = static_cast<uint8_t>(TelnetCommand::SE);
    const uint8_t NAWS = static_cast<uint8_t>(TelnetOption::NEGOTIATE_ABOUT_WINDOW_SIZE);

    QByteArray frame = makeFrame({
        IAC, SB, NAWS,        // begin subnegotiation
        0x00, IAC, IAC,       // width = 0x00FF
        0x00, IAC, IAC,       // height = 0x00FF
        IAC, SE,              // end subnegotiation
        'A', 'B', 'C'         // application data that should flow through
    });

    client.processTelnetData(frame);

    // 1. The SB buffer (snapshot just before clearing) contained the
    //    correctly-unescaped 4-byte width/height payload.
    //    handleSubnegotiation for NAWS doesn't expose the buffer to a
    //    callback, so we verify the captured buffer was cleared and that
    //    nothing leaked into the app stream.
    QCOMPARE(client.m_inSubnegotiation, false);
    QCOMPARE(client.m_subnegotiationBuffer.size(), 0);

    // 2. The app-data path must contain ONLY "ABC" — no spurious 0xFF bytes
    //    from the SB payload should have been emitted as application data.
    QByteArray received;
    for (const auto &args : spy) {
        received.append(args.at(0).toByteArray());
    }
    QCOMPARE(received, QByteArray("ABC"));
}

// Sanity check: outside an SB, IAC IAC is still application data 0xFF.
void TestTelnetSubnegotiation::testEscapedIacOutsideSubnegotiationFlowsAsAppData() {
    TN5250Client client;
    client.m_handshakeComplete = true;

    QSignalSpy spy(&client, &TN5250Client::dataReceived);

    const uint8_t IAC = static_cast<uint8_t>(TelnetCommand::IAC);
    QByteArray frame = makeFrame({'X', IAC, IAC, 'Y'});

    client.processTelnetData(frame);

    QByteArray received;
    for (const auto &args : spy) {
        received.append(args.at(0).toByteArray());
    }
    QCOMPARE(received, QByteArray::fromHex("58FF59")); // X 0xFF Y
}

// Regression tests for issue #139: the NEW_ENVIRON SEND parser must
// un-escape RFC 1572 ESC (0x02) sequences when reading the IBMRSEED
// USERVAR name, otherwise seed bytes equal to VAR/VALUE/USERVAR markers
// truncate the seed and silently disable RFC 4777 password encryption.
void TestTelnetSubnegotiation::testIbmrseedSeedWithEscapedControlBytes() {
    TN5250Client client;

    // NEW_ENVIRON SEND payload: SEND USERVAR "IBMRSEED" <8-byte seed>
    // USERVAR "IBMSUBSPW". The seed contains 0x00, 0x01, 0x02 and 0x03,
    // each ESC-escaped per RFC 1572.
    QByteArray sb;
    sb.append(char(0x01)); // SEND
    sb.append(char(0x03)); // USERVAR
    sb.append("IBMRSEED");
    const uint8_t seed[8] = {0xAA, 0x00, 0x01, 0x02, 0x03, 0xBB, 0xCC, 0xDD};
    for (uint8_t b : seed) {
        if (b == 0x00 || b == 0x01 || b == 0x02 || b == 0x03) {
            sb.append(char(0x02)); // RFC 1572 ESC
        }
        sb.append(char(b));
    }
    sb.append(char(0x03)); // USERVAR
    sb.append("IBMSUBSPW");

    client.handleSubnegotiation(TelnetOption::NEW_ENVIRON, sb);

    QCOMPARE(client.m_serverSeed.size(), 8);
    QCOMPARE(client.m_serverSeed,
             QByteArray(reinterpret_cast<const char *>(seed), 8));
}

// A seed without escapable bytes must parse exactly as before.
void TestTelnetSubnegotiation::testIbmrseedPlainSeedStillParsed() {
    TN5250Client client;

    QByteArray sb;
    sb.append(char(0x01)); // SEND
    sb.append(char(0x03)); // USERVAR
    sb.append("IBMRSEED");
    const uint8_t seed[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    sb.append(reinterpret_cast<const char *>(seed), 8);
    sb.append(char(0x03)); // USERVAR
    sb.append("IBMSUBSPW");

    client.handleSubnegotiation(TelnetOption::NEW_ENVIRON, sb);

    QCOMPARE(client.m_serverSeed,
             QByteArray(reinterpret_cast<const char *>(seed), 8));
}

// Host Integration Server 2000 (KB Q327297) sends X'FF' from the host's
// GDS data without doubling it. IAC followed by a byte below EOR cannot be a
// telnet command, so both bytes must reach the application.
void TestTelnetSubnegotiation::testUnescapedIacBeforeDataByteIsData() {
    TN5250Client client;
    client.m_handshakeComplete = true;

    QSignalSpy spy(&client, &TN5250Client::dataReceived);
    client.processTelnetData(QByteArray::fromHex("58FF4059"));

    QByteArray received;
    for (const auto &args : spy) {
        received.append(args.at(0).toByteArray());
    }
    QCOMPARE(received, QByteArray::fromHex("58FF4059"));
}

// Inside an EOR-framed record an unescaped 0xFF followed by a byte in the
// telnet command range (here 0xF1, EBCDIC '1' and also NOP) is still data.
void TestTelnetSubnegotiation::testUnescapedIacInsideRecordIsData() {
    TN5250Client client;
    client.m_handshakeComplete = true;
    client.m_eorNegotiated = true;

    QSignalSpy spy(&client, &TN5250Client::dataReceived);
    // 12-byte GDS record: header + payload FF F1, sent unescaped, then EOR.
    client.processTelnetData(QByteArray::fromHex("000C12A0000004000003FFF1FFEF"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toByteArray(), QByteArray::fromHex("000C12A0000004000003FFF1"));
}

// Records are handed over whole at IAC EOR, even across reads.
void TestTelnetSubnegotiation::testRecordsDeliveredAtEor() {
    TN5250Client client;
    client.m_handshakeComplete = true;
    client.m_eorNegotiated = true;

    QSignalSpy spy(&client, &TN5250Client::dataReceived);
    client.processTelnetData(QByteArray::fromHex("000B12A0"));
    QCOMPARE(spy.count(), 0);
    client.processTelnetData(QByteArray::fromHex("00000400000340FFEF000A12A000000400000AFFEF"));

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toByteArray(), QByteArray::fromHex("000B12A000000400000340"));
    QCOMPARE(spy.at(1).at(0).toByteArray(), QByteArray::fromHex("000A12A000000400000A"));
}

// A GDS length that disagrees with the EOR boundary must not stall the
// decoder: the length is rewritten to the record's actual size.
void TestTelnetSubnegotiation::testGdsLengthRepairedAtEor() {
    TN5250Client client;
    client.m_handshakeComplete = true;
    client.m_eorNegotiated = true;

    QSignalSpy spy(&client, &TN5250Client::dataReceived);
    // Header claims 14 bytes, only 12 arrive before EOR.
    client.processTelnetData(QByteArray::fromHex("000E12A00000040000034142FFEF"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toByteArray(), QByteArray::fromHex("000C12A0000004000003" "4142"));

    // Well-formed back-to-back records in one EOR are left alone.
    const QByteArray chained = QByteArray::fromHex("000A12A000000400000A" "000A12A000000400000A");
    QCOMPARE(TN5250Client::repairGdsRecordLength(chained), chained);
}

// Between records, telnet commands such as the NOP keep-alive still work.
void TestTelnetSubnegotiation::testNopBetweenRecordsIsCommand() {
    TN5250Client client;
    client.m_handshakeComplete = true;
    client.m_eorNegotiated = true;

    QSignalSpy spy(&client, &TN5250Client::dataReceived);
    client.processTelnetData(QByteArray::fromHex("FFF1000A12A000000400000AFFEF"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toByteArray(), QByteArray::fromHex("000A12A000000400000A"));
}

QTEST_MAIN(TestTelnetSubnegotiation)
#include "test_telnet_subnegotiation.moc"
