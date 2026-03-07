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

#include "ibmrseed.h"
#include "core/ebcdic.h"
#include "logger/logger.h"
#include <QRandomGenerator>
#include <openssl/evp.h>
#include <openssl/provider.h>

namespace tn5250::client {

// OpenSSL 3.0 moved DES to the "legacy" provider.
// Load it once so EVP_des_ecb / EVP_des_cbc work.
static bool ensureLegacyProvider() {
    static bool loaded = [] {
        OSSL_PROVIDER *legacy = OSSL_PROVIDER_load(nullptr, "legacy");
        OSSL_PROVIDER *deflt = OSSL_PROVIDER_load(nullptr, "default");
        return legacy != nullptr && deflt != nullptr;
    }();
    return loaded;
}

QByteArray IBMRSeed::encryptPassword(const QString &userId,
                                     const QString &password,
                                     const QByteArray &serverSeed,
                                     const QByteArray &clientSeed) {
    core::CodePage cp(core::CodePage::ID::CP037);
    return encryptPassword(userId, password, serverSeed, clientSeed, cp);
}

QByteArray IBMRSeed::encryptPassword(const QString &userId,
                                     const QString &password,
                                     const QByteArray &serverSeed,
                                     const QByteArray &clientSeed,
                                     const core::CodePage &cp) {
    if (serverSeed.size() != 8 || clientSeed.size() != 8) {
        logger::Logger::instance()->error(
            "[IBMRSEED]: Invalid seed size (expected 8 bytes each)");
        return {};
    }

    // Step 1: Pad password to 8 bytes EBCDIC uppercase
    QByteArray pwEBCDIC = padPasswordEBCDIC(password, cp);

    // Step 2: XOR each byte with 0x55
    QByteArray xored = xorWith55(pwEBCDIC);

    // Step 3: Left-shift entire 8-byte value by 1 bit
    QByteArray shifted = leftShift1(xored);

    // Step 4: DES-ECB encrypt userID with shifted key → PW_TOKEN
    QByteArray userIdPadded = padUserIdEBCDIC(userId, 8, cp);
    QByteArray pwToken = desEcbEncrypt(shifted, userIdPadded);
    if (pwToken.isEmpty()) {
        logger::Logger::instance()->error("[IBMRSEED]: DES-ECB encrypt failed");
        return {};
    }

    // Step 5: PWSEQs = 1 (8 bytes big-endian)
    QByteArray pwseqs(8, '\0');
    pwseqs[7] = 0x01;

    // Step 6: RDrSEQ = serverSeed + PWSEQs (big-endian addition)
    QByteArray rdrseq = addBigEndian8(serverSeed, pwseqs);

    // Step 7: Build 40-byte block and DES-CBC encrypt
    // Pad userID to 16 bytes (two 8-byte halves) for XOR with RDrSEQ
    QByteArray userId16 = padUserIdEBCDIC(userId, 16, cp);
    QByteArray userPart1 = userId16.left(8);
    QByteArray userPart2 = userId16.mid(8, 8);

    // XOR each user ID part with RDrSEQ
    QByteArray xorPart1(8, '\0');
    QByteArray xorPart2(8, '\0');
    for (int i = 0; i < 8; ++i) {
        xorPart1[i] = static_cast<char>(
            static_cast<uint8_t>(userPart1[i]) ^
            static_cast<uint8_t>(rdrseq[i]));
        xorPart2[i] = static_cast<char>(
            static_cast<uint8_t>(userPart2[i]) ^
            static_cast<uint8_t>(rdrseq[i]));
    }

    // 40-byte plaintext block
    QByteArray block;
    block.append(rdrseq);      // 8 bytes
    block.append(clientSeed);  // 8 bytes
    block.append(xorPart1);    // 8 bytes
    block.append(xorPart2);    // 8 bytes
    block.append(pwseqs);      // 8 bytes

    QByteArray iv(8, '\0'); // IV = all zeros
    QByteArray ciphertext = desCbcEncrypt(pwToken, iv, block);
    if (ciphertext.size() < 40) {
        logger::Logger::instance()->error("[IBMRSEED]: DES-CBC encrypt failed");
        return {};
    }

    // PW_SUB = last 8 bytes of ciphertext
    QByteArray pwSub = ciphertext.right(8);

    logger::Logger::instance()->debug(
        QString("[IBMRSEED]: Encrypted password (PW_SUB hex): %1")
            .arg(QString::fromLatin1(pwSub.toHex())));

    return pwSub;
}

QByteArray IBMRSeed::generateClientSeed() {
    QByteArray seed(8, '\0');
    QRandomGenerator *rng = QRandomGenerator::global();
    for (int i = 0; i < 8; ++i) {
        seed[i] = static_cast<char>(rng->bounded(256));
    }
    return seed;
}

QByteArray IBMRSeed::escapeNewEnviron(const QByteArray &data) {
    QByteArray escaped;
    escaped.reserve(data.size() * 2);
    for (int i = 0; i < data.size(); ++i) {
        uint8_t b = static_cast<uint8_t>(data[i]);
        // RFC 1572: escape VAR(0x00), VALUE(0x01), ESC(0x02),
        // USERVAR(0x03) and IAC(0xFF) by prefixing with ESC(0x02)
        if (b <= 0x03 || b == 0xFF) {
            escaped.append(static_cast<char>(0x02)); // ESC
        }
        escaped.append(data[i]);
    }
    return escaped;
}

QByteArray IBMRSeed::padPasswordEBCDIC(const QString &password) {
    core::CodePage cp(core::CodePage::ID::CP037);
    return padPasswordEBCDIC(password, cp);
}

QByteArray IBMRSeed::padPasswordEBCDIC(const QString &password, const core::CodePage &cp) {
    // Convert to uppercase, then to EBCDIC, pad/truncate to 8 bytes with 0x40
    QString upper = password.toUpper();
    QByteArray ebcdic = cp.fromUnicode(upper);

    // Truncate to 8 or pad with EBCDIC space (0x40)
    if (ebcdic.size() > 8) {
        ebcdic.truncate(8);
    }
    while (ebcdic.size() < 8) {
        ebcdic.append(static_cast<char>(0x40));
    }
    return ebcdic;
}

QByteArray IBMRSeed::xorWith55(const QByteArray &data) {
    QByteArray result(data.size(), '\0');
    for (int i = 0; i < data.size(); ++i) {
        result[i] = static_cast<char>(
            static_cast<uint8_t>(data[i]) ^ 0x55);
    }
    return result;
}

QByteArray IBMRSeed::leftShift1(const QByteArray &data) {
    if (data.size() != 8) return data;

    QByteArray result(8, '\0');
    for (int i = 0; i < 7; ++i) {
        result[i] = static_cast<char>(
            (static_cast<uint8_t>(data[i]) << 1) |
            (static_cast<uint8_t>(data[i + 1]) >> 7));
    }
    result[7] = static_cast<char>(
        static_cast<uint8_t>(data[7]) << 1);
    return result;
}

QByteArray IBMRSeed::desEcbEncrypt(const QByteArray &key,
                                   const QByteArray &plaintext) {
    if (key.size() != 8 || plaintext.size() != 8) return {};
    ensureLegacyProvider();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray out(16, '\0'); // 8 bytes + possible padding
    int outLen = 0, finalLen = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_des_ecb(), nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_set_padding(ctx, 0); // No padding

    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(out.data()),
                          &outLen,
                          reinterpret_cast<const unsigned char *>(plaintext.constData()),
                          8) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_EncryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char *>(out.data()) + outLen,
                            &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    EVP_CIPHER_CTX_free(ctx);
    out.truncate(outLen + finalLen);
    return out;
}

QByteArray IBMRSeed::desCbcEncrypt(const QByteArray &key,
                                   const QByteArray &iv,
                                   const QByteArray &plaintext) {
    if (key.size() != 8 || iv.size() != 8) return {};
    if (plaintext.size() % 8 != 0) return {};
    ensureLegacyProvider();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray out(plaintext.size() + 16, '\0');
    int outLen = 0, finalLen = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_des_cbc(), nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(out.data()),
                          &outLen,
                          reinterpret_cast<const unsigned char *>(plaintext.constData()),
                          plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_EncryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char *>(out.data()) + outLen,
                            &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    EVP_CIPHER_CTX_free(ctx);
    out.truncate(outLen + finalLen);
    return out;
}

QByteArray IBMRSeed::padUserIdEBCDIC(const QString &userId, int len) {
    core::CodePage cp(core::CodePage::ID::CP037);
    return padUserIdEBCDIC(userId, len, cp);
}

QByteArray IBMRSeed::padUserIdEBCDIC(const QString &userId, int len, const core::CodePage &cp) {
    // Convert to uppercase, then to EBCDIC, pad/truncate to len bytes with 0x40
    QString upper = userId.toUpper();
    QByteArray ebcdic = cp.fromUnicode(upper);

    if (ebcdic.size() > len) {
        ebcdic.truncate(len);
    }
    while (ebcdic.size() < len) {
        ebcdic.append(static_cast<char>(0x40));
    }
    return ebcdic;
}

QByteArray IBMRSeed::addBigEndian8(const QByteArray &a, const QByteArray &b) {
    if (a.size() != 8 || b.size() != 8) return a;

    QByteArray result(8, '\0');
    uint16_t carry = 0;
    for (int i = 7; i >= 0; --i) {
        uint16_t sum = static_cast<uint8_t>(a[i]) +
                       static_cast<uint8_t>(b[i]) + carry;
        result[i] = static_cast<char>(sum & 0xFF);
        carry = sum >> 8;
    }
    return result;
}

} // namespace tn5250::client
