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

QTEST_MAIN(TestTelnetSubnegotiation)
#include "test_telnet_subnegotiation.moc"
