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
