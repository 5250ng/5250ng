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

#include "ui/rendering/tn5250_command_handler.h"
#include <QtTest/QtTest>

using ui::rendering::TN5250CommandHandler;

// Regression test for issue #247: the Query reply put every field from the
// display indicator on one byte late, announced one input field at most,
// and declared a structured-field length that did not match what was sent.
// The layout follows lib5250's non-enhanced reply
// (tn5250_session_query_reply) with tn5250j's values (sendQueryResponse)
// for the device type, serial number, field count and bytes 49-50.
class TestQueryReply : public QObject {
    Q_OBJECT

  private slots:
    void layoutMatchesReferenceClients() {
        TN5250CommandHandler handler;
        const QByteArray r = handler.buildQueryResponse();
        QCOMPARE(r.size(), 61);
        auto at = [&r](int i) { return quint8(r[i]); };

        QCOMPARE(at(2), quint8(0x88));                    // inbound WSF AID
        // Length field counts offsets 3..60
        QCOMPARE((at(3) << 8) | at(4), r.size() - 3);
        QCOMPARE(at(5), quint8(0xD9));
        QCOMPARE(at(6), quint8(0x70));
        QCOMPARE(at(7), quint8(0x80));
        QCOMPARE(at(8), quint8(0x06));                    // controller class 0x0600
        QCOMPARE(at(9), quint8(0x00));
        for (int i = 13; i <= 28; ++i)
            QCOMPARE(at(i), quint8(0x00));                // reserved
        QCOMPARE(at(29), quint8(0x01));                   // display emulation
        QCOMPARE(r.mid(30, 7), QByteArray("\xF5\xF2\xF5\xF1\xF0\xF1\xF1", 7)); // 5251011
        QCOMPARE(at(37), quint8(0x02));                   // standard keyboard
        QCOMPARE((at(44) << 8) | at(45), 256);            // maximum input fields
        QCOMPARE(at(49), quint8(0x01));                   // capabilities
        QCOMPARE(at(50), quint8(0x11));
        // No enhanced 5250 user interface level is announced
        QCOMPARE(at(53), quint8(0x00));
        QCOMPARE(at(54), quint8(0x00));
        for (int i = 55; i < r.size(); ++i)
            QCOMPARE(at(i), quint8(0x00));

        // The whole reply, so no byte can change unnoticed
        const QByteArray expected = QByteArray::fromHex(
            "000088003ad970800600010100"
            "00000000000000000000000000000000"
            "01f5f2f5f1f0f1f1"
            "020000002424000100000000011100000000000000000000");
        QCOMPARE(expected.size(), 61);
        QCOMPARE(r.toHex(), expected.toHex());
    }
};

QTEST_MAIN(TestQueryReply)
#include "test_query_reply.moc"
