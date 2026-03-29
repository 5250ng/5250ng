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

#include "core/ebcdic.h"
#include <QtTest/QtTest>

using namespace core;

class TestEBCDIC : public QObject {
    Q_OBJECT

  private slots:
    void testEBCDICToChar();
    void testCharToEBCDIC();
    void testEBCDICToString();
    void testStringToEBCDIC();
    void testIsPrintable();
    void testRoundTrip();
    void testCommonCharacters();

  private:
};

void TestEBCDIC::testEBCDICToChar() {
    // Test common characters
    QCOMPARE(EBCDIC::ebcdicToChar(0xC1).unicode(), static_cast<uint16_t>('A'));
    QCOMPARE(EBCDIC::ebcdicToChar(0xC2).unicode(), static_cast<uint16_t>('B'));
    QCOMPARE(EBCDIC::ebcdicToChar(0xF1).unicode(), static_cast<uint16_t>('1'));
    QCOMPARE(EBCDIC::ebcdicToChar(0xF2).unicode(), static_cast<uint16_t>('2'));
    QCOMPARE(EBCDIC::ebcdicToChar(0x40).unicode(), static_cast<uint16_t>(' ')); // Space
}

void TestEBCDIC::testCharToEBCDIC() {
    // Test common characters
    QCOMPARE(EBCDIC::charToEBCDIC('A'), static_cast<uint8_t>(0xC1));
    QCOMPARE(EBCDIC::charToEBCDIC('B'), static_cast<uint8_t>(0xC2));
    QCOMPARE(EBCDIC::charToEBCDIC('1'), static_cast<uint8_t>(0xF1));
    QCOMPARE(EBCDIC::charToEBCDIC('2'), static_cast<uint8_t>(0xF2));
    QCOMPARE(EBCDIC::charToEBCDIC(' '), static_cast<uint8_t>(0x40)); // Space
}

void TestEBCDIC::testEBCDICToString() {
    QByteArray ebcdic;
    ebcdic.append(static_cast<char>(0xC1)); // 'A'
    ebcdic.append(static_cast<char>(0xC2)); // 'B'
    ebcdic.append(static_cast<char>(0xC3)); // 'C'

    QString result = EBCDIC::ebcdicToString(ebcdic);
    QCOMPARE(result, QString("ABC"));
}

void TestEBCDIC::testStringToEBCDIC() {
    QString str = "ABC";
    QByteArray result = EBCDIC::stringToEBCDIC(str);

    QCOMPARE(result.size(), 3);
    QCOMPARE(static_cast<uint8_t>(result[0]), static_cast<uint8_t>(0xC1));
    QCOMPARE(static_cast<uint8_t>(result[1]), static_cast<uint8_t>(0xC2));
    QCOMPARE(static_cast<uint8_t>(result[2]), static_cast<uint8_t>(0xC3));
}

void TestEBCDIC::testIsPrintable() {
    QVERIFY(EBCDIC::isPrintable(0xC1));  // 'A'
    QVERIFY(EBCDIC::isPrintable(0xF1));  // '1'
    QVERIFY(EBCDIC::isPrintable(0x40));  // Space
    QVERIFY(!EBCDIC::isPrintable(0x00)); // Null
    QVERIFY(!EBCDIC::isPrintable(0x0A)); // Line feed (might be printable in some contexts)
}

void TestEBCDIC::testRoundTrip() {
    // Test that converting to EBCDIC and back preserves common characters
    QString original = "HELLO WORLD 123";
    QByteArray ebcdic = EBCDIC::stringToEBCDIC(original);
    QString converted = EBCDIC::ebcdicToString(ebcdic);

    QCOMPARE(converted, original);
}

void TestEBCDIC::testCommonCharacters() {
    // Test full alphabet
    QString alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QByteArray ebcdic = EBCDIC::stringToEBCDIC(alphabet);
    QString result = EBCDIC::ebcdicToString(ebcdic);
    QCOMPARE(result, alphabet);

    // Test numbers
    QString numbers = "0123456789";
    ebcdic = EBCDIC::stringToEBCDIC(numbers);
    result = EBCDIC::ebcdicToString(ebcdic);
    QCOMPARE(result, numbers);
}

QTEST_MAIN(TestEBCDIC)
#include "test_ebcdic.moc"
