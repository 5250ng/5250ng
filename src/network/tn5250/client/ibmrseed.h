#pragma once

#include "core/codepage.h"
#include <QByteArray>
#include <QString>
#include <cstdint>

namespace tn5250::client {

// RFC 4777 IBMRSEED password encryption for TN5250E
// Implements the DES-based password substitution algorithm (QPWDLVL 0-1)
class IBMRSeed {
  public:
    // Encrypt password per RFC 4777 Section 5 (DES method)
    // Returns the 8-byte PW_SUB value, or empty on failure
    static QByteArray encryptPassword(const QString &userId,
                                      const QString &password,
                                      const QByteArray &serverSeed,
                                      const QByteArray &clientSeed);

    // Overload using an explicit code page (thread-safe, no global state)
    static QByteArray encryptPassword(const QString &userId,
                                      const QString &password,
                                      const QByteArray &serverSeed,
                                      const QByteArray &clientSeed,
                                      const core::CodePage &cp);

    // Generate a random 8-byte client seed
    static QByteArray generateClientSeed();

    // Escape binary data for RFC 1572 NEW_ENVIRON subnegotiation
    // Bytes 0x00 (VAR), 0x01 (VALUE), 0x02 (ESC), 0x03 (USERVAR),
    // and 0xFF (IAC) must be prefixed with ESC (0x02)
    static QByteArray escapeNewEnviron(const QByteArray &data);

  private:
    // Step 1: Convert password to 8-byte EBCDIC uppercase, padded with 0x40
    static QByteArray padPasswordEBCDIC(const QString &password);
    static QByteArray padPasswordEBCDIC(const QString &password, const core::CodePage &cp);

    // Step 2: XOR each byte with 0x55
    static QByteArray xorWith55(const QByteArray &data);

    // Step 3: Left-shift entire 8-byte value by 1 bit
    static QByteArray leftShift1(const QByteArray &data);

    // Step 4: DES-ECB encrypt (key=shifted password, plaintext=padded userID)
    static QByteArray desEcbEncrypt(const QByteArray &key,
                                    const QByteArray &plaintext);

    // Step 7: DES-CBC encrypt 40-byte block
    static QByteArray desCbcEncrypt(const QByteArray &key,
                                    const QByteArray &iv,
                                    const QByteArray &plaintext);

    // Pad user ID to 8 bytes EBCDIC uppercase
    static QByteArray padUserIdEBCDIC(const QString &userId, int len);
    static QByteArray padUserIdEBCDIC(const QString &userId, int len, const core::CodePage &cp);

    // Big-endian 8-byte addition: a + b
    static QByteArray addBigEndian8(const QByteArray &a, const QByteArray &b);
};

} // namespace tn5250::client
