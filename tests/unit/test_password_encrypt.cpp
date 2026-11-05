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
#include "network/hostserver/signon_client.h"
#include <QByteArray>
#include <QSignalSpy>
#include <QString>
#include <QTest>

using namespace hostserver;

class TestPasswordEncrypt : public QObject {
    Q_OBJECT

  private slots:
    void testGenerateSeed() {
        QByteArray seed1 = PasswordEncrypt::generateSeed();
        QByteArray seed2 = PasswordEncrypt::generateSeed();

        QCOMPARE(seed1.size(), 8);
        QCOMPARE(seed2.size(), 8);
        // Seeds should be random (extremely unlikely to be equal)
        QVERIFY(seed1 != seed2);
    }

    void testAuthScheme() {
        QCOMPARE(PasswordEncrypt::authScheme(0), static_cast<uint8_t>(0x01)); // DES
        QCOMPARE(PasswordEncrypt::authScheme(1), static_cast<uint8_t>(0x01)); // DES
        QCOMPARE(PasswordEncrypt::authScheme(2), static_cast<uint8_t>(0x03)); // SHA-1
        QCOMPARE(PasswordEncrypt::authScheme(3), static_cast<uint8_t>(0x03)); // SHA-1
        // Levels >= 4 (QPWDLVL 4, SHA-512/PBKDF2) are not implemented:
        // encrypt() falls back to SHA-1, so the declared scheme must match
        // the algorithm actually used (issue #149).
        QCOMPARE(PasswordEncrypt::authScheme(4), static_cast<uint8_t>(0x03)); // SHA-1 fallback
    }

    void testEncryptDES() {
        QByteArray clientSeed = PasswordEncrypt::generateSeed();
        QByteArray serverSeed = PasswordEncrypt::generateSeed();

        QByteArray result = PasswordEncrypt::encryptDES(
            QStringLiteral("MYUSER"), QStringLiteral("MYPASS"),
            clientSeed, serverSeed);

        // DES produces 8-byte result
        QCOMPARE(result.size(), 8);
        // Should not be all zeros
        QVERIFY(result != QByteArray(8, '\0'));
    }

    void testEncryptDESReproducible() {
        QByteArray clientSeed(8, '\0');
        clientSeed[7] = 0x42;
        QByteArray serverSeed(8, '\0');
        serverSeed[7] = 0x37;

        QByteArray result1 = PasswordEncrypt::encryptDES(
            QStringLiteral("USER1"), QStringLiteral("PASS1"),
            clientSeed, serverSeed);
        QByteArray result2 = PasswordEncrypt::encryptDES(
            QStringLiteral("USER1"), QStringLiteral("PASS1"),
            clientSeed, serverSeed);

        // Same inputs = same output
        QCOMPARE(result1, result2);
    }

    void testEncryptDESDifferentSeeds() {
        QByteArray clientSeed1 = PasswordEncrypt::generateSeed();
        QByteArray serverSeed1 = PasswordEncrypt::generateSeed();
        QByteArray clientSeed2 = PasswordEncrypt::generateSeed();
        QByteArray serverSeed2 = PasswordEncrypt::generateSeed();

        QByteArray result1 = PasswordEncrypt::encryptDES(
            QStringLiteral("USER"), QStringLiteral("PASS"),
            clientSeed1, serverSeed1);
        QByteArray result2 = PasswordEncrypt::encryptDES(
            QStringLiteral("USER"), QStringLiteral("PASS"),
            clientSeed2, serverSeed2);

        // Different seeds = different output (virtually certain)
        QVERIFY(result1 != result2);
    }

    void testEncryptSHA1() {
        QByteArray clientSeed = PasswordEncrypt::generateSeed();
        QByteArray serverSeed = PasswordEncrypt::generateSeed();

        QByteArray result = PasswordEncrypt::encryptSHA1(
            QStringLiteral("MYUSER"), QStringLiteral("MYPASS"),
            clientSeed, serverSeed);

        // SHA-1 produces 20-byte result
        QCOMPARE(result.size(), 20);
        QVERIFY(result != QByteArray(20, '\0'));
    }

    void testEncryptSHA1Reproducible() {
        QByteArray clientSeed(8, '\0');
        clientSeed[0] = 0x11;
        QByteArray serverSeed(8, '\0');
        serverSeed[0] = 0x22;

        QByteArray result1 = PasswordEncrypt::encryptSHA1(
            QStringLiteral("ADMIN"), QStringLiteral("SECRET"),
            clientSeed, serverSeed);
        QByteArray result2 = PasswordEncrypt::encryptSHA1(
            QStringLiteral("ADMIN"), QStringLiteral("SECRET"),
            clientSeed, serverSeed);

        QCOMPARE(result1, result2);
    }

    // Vectors from IBM JTOpen 21.0.7 (AS400ImplRemote.generateShaToken and
    // generateShaSubstitute, with the user ID built as JTOpen builds it:
    // blank-padded to 10 characters, UTF-16BE). Issue #444.
    void testEncryptSHA1JTOpenVectors_data() {
        QTest::addColumn<QString>("userId");
        QTest::addColumn<QString>("password");
        QTest::addColumn<QByteArray>("expected");
        QTest::newRow("7-char ID") << "QSECOFR" << "QSECOFR0"
                                   << QByteArray::fromHex("b14f1aa9bb3ff62cb6d22af3c0498692de58fd9e");
        QTest::newRow("1-char ID") << "A" << "b"
                                   << QByteArray::fromHex("3a405dee6d7c11a23680b34c3321fe5af0c59b46");
        QTest::newRow("mixed-case password")
            << "TESTUSER" << "Secret1" << QByteArray::fromHex("59df75f320a9bb9b72ffd35a1a3755c15c262ce3");
        QTest::newRow("10-char ID") << "ABCDEFGHIJ" << "password"
                                    << QByteArray::fromHex("632212ac0d94207404ad76f15daa0a4b4c14bda5");
        QTest::newRow("long password")
            << "USER" << "LongPassword123" << QByteArray::fromHex("273d6f34359ed19fdd50da8ef9b71cd8ec979d0d");
        QTest::newRow("lower-case ID") << "qsecofr" << "QSECOFR0"
                                       << QByteArray::fromHex("b14f1aa9bb3ff62cb6d22af3c0498692de58fd9e");
    }

    void testEncryptSHA1JTOpenVectors() {
        QFETCH(QString, userId);
        QFETCH(QString, password);
        QFETCH(QByteArray, expected);
        const QByteArray client = QByteArray::fromHex("0123456789abcdef");
        const QByteArray server = QByteArray::fromHex("fedcba9876543210");
        QCOMPARE(PasswordEncrypt::encryptSHA1(userId, password, client, server).toHex(), expected.toHex());
    }

    // A user ID the wire would truncate gets no substitute
    void testEncryptSHA1RejectsLongUserId() {
        const QByteArray seed = QByteArray::fromHex("0123456789abcdef");
        QVERIFY(PasswordEncrypt::encryptSHA1(QStringLiteral("ABCDEFGHIJK"), QStringLiteral("pw"), seed, seed)
                    .isEmpty());
    }

    void testEncryptDESRejectsLongUserId() {
        const QByteArray seed = QByteArray::fromHex("0123456789abcdef");
        QVERIFY(PasswordEncrypt::encryptDES(QStringLiteral("ABCDEFGHIJK"), QStringLiteral("pw"), seed, seed)
                    .isEmpty());
        // Ten characters is the limit, not past it
        QCOMPARE(PasswordEncrypt::encryptDES(QStringLiteral("ABCDEFGHIJ"), QStringLiteral("pw"), seed, seed)
                     .size(),
                 8);
    }

    void testSignonRejectsLongUserIdBeforeConnecting() {
        SignonClient client;
        QSignalSpy errors(&client, &SignonClient::errorOccurred);
        // A host that would only fail later; the reject must come first
        client.signon(QStringLiteral("192.0.2.1"), 8476, QStringLiteral("ABCDEFGHIJK"), QStringLiteral("pw"));
        QCOMPARE(errors.count(), 1);
        QCOMPARE(errors.at(0).at(0).toString(), QStringLiteral("User ID longer than 10 characters"));
        QCOMPARE(client.state(), SignonClient::State::Error);
    }

    void testLongAfterUpperCaseIsRejected() {
        // Ten characters that upper-case to eleven: "ABCDEFGHIß" -> "ABCDEFGHISS"
        const QString user = QStringLiteral("ABCDEFGHI") + QChar(0x00DF);
        QCOMPARE(user.size(), 10);
        const QByteArray seed = QByteArray::fromHex("0123456789abcdef");
        QVERIFY(PasswordEncrypt::encryptDES(user, QStringLiteral("pw"), seed, seed).isEmpty());
        QVERIFY(PasswordEncrypt::encryptSHA1(user, QStringLiteral("pw"), seed, seed).isEmpty());
        SignonClient client;
        QSignalSpy errors(&client, &SignonClient::errorOccurred);
        client.signon(QStringLiteral("192.0.2.1"), 8476, user, QStringLiteral("pw"));
        QCOMPARE(errors.count(), 1);
        QCOMPARE(client.state(), SignonClient::State::Error);
    }

    void testEncryptAutoSelectDES() {
        QByteArray clientSeed = PasswordEncrypt::generateSeed();
        QByteArray serverSeed = PasswordEncrypt::generateSeed();

        QByteArray result = PasswordEncrypt::encrypt(
            QStringLiteral("USER"), QStringLiteral("PASS"),
            clientSeed, serverSeed, 0);

        QCOMPARE(result.size(), 8); // DES = 8 bytes
    }

    void testEncryptAutoSelectSHA1() {
        QByteArray clientSeed = PasswordEncrypt::generateSeed();
        QByteArray serverSeed = PasswordEncrypt::generateSeed();

        QByteArray result = PasswordEncrypt::encrypt(
            QStringLiteral("USER"), QStringLiteral("PASS"),
            clientSeed, serverSeed, 2);

        QCOMPARE(result.size(), 20); // SHA-1 = 20 bytes
    }

    void testEncryptInvalidSeedSize() {
        QByteArray shortSeed(4, '\0');
        QByteArray normalSeed(8, '\0');

        // Client seed too short
        QByteArray result1 = PasswordEncrypt::encryptDES(
            QStringLiteral("U"), QStringLiteral("P"), shortSeed, normalSeed);
        QVERIFY(result1.isEmpty());

        // Server seed too short
        QByteArray result2 = PasswordEncrypt::encryptSHA1(
            QStringLiteral("U"), QStringLiteral("P"), normalSeed, shortSeed);
        QVERIFY(result2.isEmpty());
    }
};

QTEST_MAIN(TestPasswordEncrypt)
#include "test_password_encrypt.moc"
