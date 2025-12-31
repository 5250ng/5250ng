#include <QtTest/QtTest>
#include "core/ebcdic.h"

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
    ebcdic.append(0xC1); // 'A'
    ebcdic.append(0xC2); // 'B'
    ebcdic.append(0xC3); // 'C'
    
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
    QVERIFY(EBCDIC::isPrintable(0xC1)); // 'A'
    QVERIFY(EBCDIC::isPrintable(0xF1)); // '1'
    QVERIFY(EBCDIC::isPrintable(0x40)); // Space
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

