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

#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

namespace hostserver {

// Password encryption for IBM i host server authentication.
// Supports DES (QPWDLVL 0-1) and SHA-1 (QPWDLVL 2-3) methods.
//
// The encryption uses client and server random seeds exchanged during
// the handshake to produce a one-time password substitute.
class PasswordEncrypt {
  public:
    // Encrypt password using DES method (password level 0-1).
    // Returns 8-byte encrypted password or empty on failure.
    static QByteArray encryptDES(const QString &userId,
                                 const QString &password,
                                 const QByteArray &clientSeed,
                                 const QByteArray &serverSeed);

    // Encrypt password using SHA-1 method (password level 2-3).
    // Returns 20-byte encrypted password or empty on failure.
    static QByteArray encryptSHA1(const QString &userId,
                                  const QString &password,
                                  const QByteArray &clientSeed,
                                  const QByteArray &serverSeed);

    // Auto-select encryption based on password level from server.
    // passwordLevel: 0-1 = DES, 2-3 = SHA-1
    static QByteArray encrypt(const QString &userId,
                              const QString &password,
                              const QByteArray &clientSeed,
                              const QByteArray &serverSeed,
                              uint8_t passwordLevel);

    // Returns the auth scheme byte for the given password level
    static uint8_t authScheme(uint8_t passwordLevel);

    // Generate a random 8-byte seed
    static QByteArray generateSeed();

  private:
    // DES helpers (reusing the IBMRSEED approach)
    static QByteArray padPasswordEBCDIC(const QString &password);
    static QByteArray padUserIdEBCDIC(const QString &userId, int len);
    static QByteArray xorWith55(const QByteArray &data);
    static QByteArray leftShift1(const QByteArray &data);
    static QByteArray desEcbEncrypt(const QByteArray &key, const QByteArray &plaintext);
    static QByteArray desCbcEncrypt(const QByteArray &key, const QByteArray &iv,
                                    const QByteArray &plaintext);
    static QByteArray addBigEndian8(const QByteArray &a, const QByteArray &b);

    // SHA-1 helper
    static QByteArray sha1(const QByteArray &data);
};

} // namespace hostserver
