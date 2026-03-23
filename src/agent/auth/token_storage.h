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

#include <QDateTime>
#include <QString>

namespace agent {

/// Persists OAuth tokens in QSettings.
///
/// Note: For production use with sensitive tokens, consider integrating
/// qtkeychain for OS-native secret storage (Keyring/Keychain/Credential Manager).
/// This implementation uses QSettings as a portable baseline.
class TokenStorage {
  public:
    static TokenStorage &instance();

    void saveTokens(const QString &providerId, const QString &accessToken,
                    const QString &refreshToken, const QDateTime &expiresAt);

    bool loadTokens(const QString &providerId, QString &accessToken,
                    QString &refreshToken, QDateTime &expiresAt) const;

    void clearTokens(const QString &providerId);

  private:
    TokenStorage() = default;
};

} // namespace agent
