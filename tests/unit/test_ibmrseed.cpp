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

#include "network/tn5250/client/ibmrseed.h"
#include <QByteArray>
#include <QTest>

using tn5250::client::IBMRSeed;

class TestIBMRSeed : public QObject {
    Q_OBJECT

  private slots:
    void testRFC4777Vector();
    void testEscapeNewEnviron();
    void testGenerateClientSeed();
};

void TestIBMRSeed::testRFC4777Vector() {
    // RFC 4777 Section 5 test vector
    QString userId = "USER123";
    QString password = "ABCDEFG";
    QByteArray serverSeed = QByteArray::fromHex("7D4C2319F28004B2");
    QByteArray clientSeed = QByteArray::fromHex("08BEF662D851F4B1");
    QByteArray expected = QByteArray::fromHex("5A58BD50E4DD9B5F");

    QByteArray result = IBMRSeed::encryptPassword(userId, password,
                                                  serverSeed, clientSeed);

    QCOMPARE(result.toHex(), expected.toHex());
}

void TestIBMRSeed::testEscapeNewEnviron() {
    // Test escaping of control bytes
    QByteArray input;
    input.append(static_cast<char>(0x00)); // VAR
    input.append(static_cast<char>(0x01)); // VALUE
    input.append(static_cast<char>(0x02)); // ESC
    input.append(static_cast<char>(0x03)); // USERVAR
    input.append(static_cast<char>(0x04)); // regular byte
    input.append(static_cast<char>(0xFF)); // IAC

    QByteArray escaped = IBMRSeed::escapeNewEnviron(input);

    // Each of bytes 0x00-0x03 and 0xFF should be prefixed with 0x02
    // 0x04 should remain as-is
    QByteArray expected;
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0x00));
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0x01));
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0x02));
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0x03));
    expected.append(static_cast<char>(0x04));
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0xFF));

    QCOMPARE(escaped.toHex(), expected.toHex());
}

void TestIBMRSeed::testGenerateClientSeed() {
    QByteArray seed1 = IBMRSeed::generateClientSeed();
    QByteArray seed2 = IBMRSeed::generateClientSeed();

    QCOMPARE(seed1.size(), 8);
    QCOMPARE(seed2.size(), 8);
    // Seeds should (almost certainly) be different
    QVERIFY(seed1 != seed2);
}

QTEST_MAIN(TestIBMRSeed)
#include "test_ibmrseed.moc"
