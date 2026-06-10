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
#include <QByteArray>
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
