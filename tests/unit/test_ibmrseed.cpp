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

#include "network/hostserver/password_encrypt.h"
#include "network/tn5250_qt/client/ibmrseed.h"
#include "logger/logger.h"
#include <QByteArray>
#include <QTest>

using tn5250::client::IBMRSeed;

class TestIBMRSeed : public QObject {
    Q_OBJECT

  private slots:
    void testRFC4777Vector();
    void testJTOpenVectors_data();
    void testJTOpenVectors();
    void testEscapeNewEnviron();
    void testGenerateClientSeed();
    void testEncryptPasswordDoesNotLogPasswordSubstitute();
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

    // Each of bytes 0x00-0x03 should be prefixed with 0x02, 0xFF (IAC)
    // doubled (RFC 855), and 0x04 should remain as-is
    QByteArray expected;
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0x00));
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0x01));
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0x02));
    expected.append(static_cast<char>(0x02)); expected.append(static_cast<char>(0x03));
    expected.append(static_cast<char>(0x04));
    expected.append(static_cast<char>(0xFF)); expected.append(static_cast<char>(0xFF));

    QCOMPARE(escaped.toHex(), expected.toHex());

    // Consecutive IACs, as in a binary seed: every one is doubled and none
    // is preceded by ESC.
    QCOMPARE(IBMRSeed::escapeNewEnviron(QByteArray::fromHex("11ffff35")).toHex(),
             QByteArray("11ffffffff35"));
}

void TestIBMRSeed::testGenerateClientSeed() {
    QByteArray seed1 = IBMRSeed::generateClientSeed();
    QByteArray seed2 = IBMRSeed::generateClientSeed();

    QCOMPARE(seed1.size(), 8);
    QCOMPARE(seed2.size(), 8);
    // Seeds should (almost certainly) be different
    QVERIFY(seed1 != seed2);
}

// The password substitute and the seeds are enough for an offline attack on
// the password, so debug logging (which can be saved and sent with a bug
// report, or read over MCP) must not contain it.
void TestIBMRSeed::testEncryptPasswordDoesNotLogPasswordSubstitute() {
    auto *log = logger::Logger::instance();
    log->enableConsoleOutput(false);
    log->setLogLevel(logger::LogLevel::Debug);

    QByteArray pwSub = IBMRSeed::encryptPassword(
        "USER123", "ABCDEFG", QByteArray::fromHex("7D4C2319F28004B2"),
        QByteArray::fromHex("08BEF662D851F4B1"));
    QCOMPARE(pwSub.size(), 8);

    const QString needle = QString::fromLatin1(pwSub.toHex());
    for (const QString &line : log->recentLogs())
        QVERIFY2(!line.contains(needle, Qt::CaseInsensitive), qPrintable(line));

    log->setLogLevel(logger::LogLevel::Info);
    log->enableConsoleOutput(true);
}

// Issue #273: user IDs and passwords of 9-10 characters were truncated to
// 8. Expected substitutes come from IBM JTOpen's own
// AS400ImplRemote.encryptPassword (jt400 21.0.7, called by reflection with
// blank-padded 10-byte EBCDIC buffers), client seed 0123456789abcdef and
// server seed fedcba9876543210.
void TestIBMRSeed::testJTOpenVectors_data() {
    QTest::addColumn<QString>("user");
    QTest::addColumn<QString>("password");
    QTest::addColumn<QByteArray>("expected");
    QTest::newRow("10-char user, 9-char pw") << "ABCDEFGHIJ" << "PASSWORD1" << QByteArray("cff50bafd0d635af");
    QTest::newRow("short user, 10-char pw") << "USER" << "PASSWORD12" << QByteArray("f356f58b475c5d6d");
    QTest::newRow("10-char user, short pw") << "LONGUSER10" << "SHORT" << QByteArray("929be830527ab925");
    QTest::newRow("9-char user, 10-char pw") << "ABCDEFGHI" << "ABCDEFGHIJ" << QByteArray("e5a7cef17ccc66ca");
    QTest::newRow("8-char pw") << "QSECOFR" << "QSECOFR0" << QByteArray("896efc7641988cfe");
    QTest::newRow("8-char user") << "TESTUSER" << "SECRET" << QByteArray("540263918ae73337");
    QTest::newRow("1-char both") << "A" << "B" << QByteArray("55145ebdbfd198bd");
    // JTOpen ignores what follows a blank, and user ID characters past 10
    QTest::newRow("blank in password") << "ABCDEFGHIJ" << "AB CD" << QByteArray("4992b451642d3649");
    QTest::newRow("11-char user") << "ABCDEFGHIJK" << "PASSWORD1" << QByteArray("cff50bafd0d635af");
}

void TestIBMRSeed::testJTOpenVectors() {
    QFETCH(QString, user);
    QFETCH(QString, password);
    QFETCH(QByteArray, expected);
    const QByteArray client = QByteArray::fromHex("0123456789abcdef");
    const QByteArray server = QByteArray::fromHex("fedcba9876543210");
    // RFC 4777 IBMRSEED (telnet) and the host server sign-on share the DES
    // substitute
    QCOMPARE(IBMRSeed::encryptPassword(user, password, server, client).toHex(), expected);
    // The host server sign-on rejects an ID past 10 characters instead, as
    // JTOpen's AS400 does before reaching this primitive (#450)
    if (user.size() > 10)
        QVERIFY(hostserver::PasswordEncrypt::encryptDES(user, password, client, server).isEmpty());
    else
        QCOMPARE(hostserver::PasswordEncrypt::encryptDES(user, password, client, server).toHex(), expected);
    // Lower case is folded to upper case as the host does at QPWDLVL 0-1
    QCOMPARE(IBMRSeed::encryptPassword(user.toLower(), password.toLower(), server, client).toHex(),
             expected);
}

QTEST_MAIN(TestIBMRSeed)
#include "test_ibmrseed.moc"
