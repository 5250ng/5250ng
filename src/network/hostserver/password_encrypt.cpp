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

#include "password_encrypt.h"
#include "core/codepage.h"
#include "host_data_stream.h"
#include "logger/logger.h"
#include <QRandomGenerator>
#include <openssl/evp.h>
#include <openssl/provider.h>

namespace hostserver {

static bool ensureLegacyProvider() {
    static bool loaded = [] {
        OSSL_PROVIDER *legacy = OSSL_PROVIDER_load(nullptr, "legacy");
        OSSL_PROVIDER *deflt = OSSL_PROVIDER_load(nullptr, "default");
        return legacy != nullptr && deflt != nullptr;
    }();
    return loaded;
}

QByteArray PasswordEncrypt::generateSeed() {
    QByteArray seed(8, '\0');
    QRandomGenerator *rng = QRandomGenerator::global();
    for (int i = 0; i < 8; ++i) {
        seed[i] = static_cast<char>(rng->bounded(256));
    }
    return seed;
}

uint8_t PasswordEncrypt::authScheme(uint8_t passwordLevel) {
    if (passwordLevel <= 1) return 0x01; // DES
    if (passwordLevel <= 3) return 0x03; // SHA-1
    return 0x07; // SHA-512/PBKDF2
}

QByteArray PasswordEncrypt::encrypt(const QString &userId,
                                    const QString &password,
                                    const QByteArray &clientSeed,
                                    const QByteArray &serverSeed,
                                    uint8_t passwordLevel) {
    if (passwordLevel <= 1) {
        return encryptDES(userId, password, clientSeed, serverSeed);
    }
    return encryptSHA1(userId, password, clientSeed, serverSeed);
}

// ---- DES encryption (QPWDLVL 0-1) ----
// Identical algorithm to IBMRSeed but self-contained here

QByteArray PasswordEncrypt::encryptDES(const QString &userId,
                                       const QString &password,
                                       const QByteArray &clientSeed,
                                       const QByteArray &serverSeed) {
    if (clientSeed.size() != 8 || serverSeed.size() != 8) {
        LOG_ERROR("[PasswordEncrypt]: Invalid seed size for DES");
        return {};
    }

    QByteArray pwEBCDIC = padPasswordEBCDIC(password);
    QByteArray xored = xorWith55(pwEBCDIC);
    QByteArray shifted = leftShift1(xored);

    QByteArray userIdPadded = padUserIdEBCDIC(userId, 8);
    QByteArray pwToken = desEcbEncrypt(shifted, userIdPadded);
    if (pwToken.isEmpty()) {
        LOG_ERROR("[PasswordEncrypt]: DES-ECB encrypt failed");
        return {};
    }

    QByteArray pwseqs(8, '\0');
    pwseqs[7] = 0x01;

    QByteArray rdrseq = addBigEndian8(serverSeed, pwseqs);

    QByteArray userId16 = padUserIdEBCDIC(userId, 16);
    QByteArray userPart1 = userId16.left(8);
    QByteArray userPart2 = userId16.mid(8, 8);

    QByteArray xorPart1(8, '\0');
    QByteArray xorPart2(8, '\0');
    for (int i = 0; i < 8; ++i) {
        xorPart1[i] = static_cast<char>(
            static_cast<uint8_t>(userPart1[i]) ^ static_cast<uint8_t>(rdrseq[i]));
        xorPart2[i] = static_cast<char>(
            static_cast<uint8_t>(userPart2[i]) ^ static_cast<uint8_t>(rdrseq[i]));
    }

    QByteArray block;
    block.append(rdrseq);
    block.append(clientSeed);
    block.append(xorPart1);
    block.append(xorPart2);
    block.append(pwseqs);

    QByteArray iv(8, '\0');
    QByteArray ciphertext = desCbcEncrypt(pwToken, iv, block);
    if (ciphertext.size() < 40) {
        LOG_ERROR("[PasswordEncrypt]: DES-CBC encrypt failed");
        return {};
    }

    return ciphertext.right(8);
}

// ---- SHA-1 encryption (QPWDLVL 2-3) ----

QByteArray PasswordEncrypt::encryptSHA1(const QString &userId,
                                        const QString &password,
                                        const QByteArray &clientSeed,
                                        const QByteArray &serverSeed) {
    if (clientSeed.size() != 8 || serverSeed.size() != 8) {
        LOG_ERROR("[PasswordEncrypt]: Invalid seed size for SHA-1");
        return {};
    }

    // Step 1: Generate password token = SHA-1(userID_utf16 + password_utf16)
    QByteArray userIdUtf16 = HostDataStream::toUtf16BE(userId.toUpper());
    QByteArray passwordUtf16 = HostDataStream::toUtf16BE(password);

    QByteArray tokenInput;
    tokenInput.append(userIdUtf16);
    tokenInput.append(passwordUtf16);
    QByteArray token = sha1(tokenInput);

    // Step 2: Generate substitute = SHA-1(token + serverSeed + clientSeed + userID_utf16 + seq)
    QByteArray sequence(8, '\0');
    sequence[7] = 0x01;

    QByteArray subInput;
    subInput.append(token);
    subInput.append(serverSeed);
    subInput.append(clientSeed);
    subInput.append(userIdUtf16);
    subInput.append(sequence);

    QByteArray result = sha1(subInput);

    LOG_DEBUG(QString("[PasswordEncrypt]: SHA-1 encrypted password (%1 bytes)")
                  .arg(result.size()));
    return result;
}

// ---- Private helpers ----

QByteArray PasswordEncrypt::padPasswordEBCDIC(const QString &password) {
    core::CodePage cp(core::CodePage::ID::CP037);
    QString upper = password.toUpper();
    QByteArray ebcdic = cp.fromUnicode(upper);
    if (ebcdic.size() > 8) ebcdic.truncate(8);
    while (ebcdic.size() < 8) ebcdic.append(static_cast<char>(0x40));
    return ebcdic;
}

QByteArray PasswordEncrypt::padUserIdEBCDIC(const QString &userId, int len) {
    core::CodePage cp(core::CodePage::ID::CP037);
    QString upper = userId.toUpper();
    QByteArray ebcdic = cp.fromUnicode(upper);
    if (ebcdic.size() > len) ebcdic.truncate(len);
    while (ebcdic.size() < len) ebcdic.append(static_cast<char>(0x40));
    return ebcdic;
}

QByteArray PasswordEncrypt::xorWith55(const QByteArray &data) {
    QByteArray result(data.size(), '\0');
    for (int i = 0; i < data.size(); ++i) {
        result[i] = static_cast<char>(static_cast<uint8_t>(data[i]) ^ 0x55);
    }
    return result;
}

QByteArray PasswordEncrypt::leftShift1(const QByteArray &data) {
    if (data.size() != 8) return data;
    QByteArray result(8, '\0');
    for (int i = 0; i < 7; ++i) {
        result[i] = static_cast<char>(
            (static_cast<uint8_t>(data[i]) << 1) |
            (static_cast<uint8_t>(data[i + 1]) >> 7));
    }
    result[7] = static_cast<char>(static_cast<uint8_t>(data[7]) << 1);
    return result;
}

QByteArray PasswordEncrypt::desEcbEncrypt(const QByteArray &key,
                                          const QByteArray &plaintext) {
    if (key.size() != 8 || plaintext.size() != 8) return {};
    ensureLegacyProvider();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray out(16, '\0');
    int outLen = 0, finalLen = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_des_ecb(), nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    EVP_CIPHER_CTX_set_padding(ctx, 0);

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

QByteArray PasswordEncrypt::desCbcEncrypt(const QByteArray &key,
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

QByteArray PasswordEncrypt::addBigEndian8(const QByteArray &a, const QByteArray &b) {
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

QByteArray PasswordEncrypt::sha1(const QByteArray &data) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return {};

    QByteArray digest(20, '\0');
    unsigned int digestLen = 0;

    if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.constData(), static_cast<size_t>(data.size())) != 1 ||
        EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char *>(digest.data()),
                           &digestLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    EVP_MD_CTX_free(ctx);
    digest.truncate(static_cast<int>(digestLen));
    return digest;
}

} // namespace hostserver
