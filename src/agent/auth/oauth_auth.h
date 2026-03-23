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

#include "auth_method.h"
#include "oauth_config.h"
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QTimer>

namespace agent {

/// OAuth2 authentication using Authorization Code + PKCE.
///
/// Holds access/refresh tokens, applies Bearer auth to requests,
/// and proactively refreshes tokens before expiry.
class OAuthAuth : public AuthMethod {
    Q_OBJECT

  public:
    explicit OAuthAuth(const OAuthConfig &config, const QString &providerId,
                       QObject *parent = nullptr);

    AuthType type() const override { return AuthType::OAuth; }
    bool isAuthenticated() const override;
    void applyAuth(QNetworkRequest &request) override;
    void authenticate() override;
    void logout() override;

    /// Load tokens from persistent storage.
    void loadFromStorage();

    /// Attempt to refresh the access token using the refresh token.
    /// Emits tokenRefreshed() on success, authenticationFailed() on failure.
    void refreshAccessToken();

    QString providerId() const { return m_providerId; }

  private:
    void onTokensReceived(const QString &accessToken, const QString &refreshToken, int expiresIn);
    void scheduleRefresh();

    OAuthConfig m_config;
    QString m_providerId;
    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_expiresAt;
    QNetworkAccessManager m_nam;
    QTimer m_refreshTimer;
};

} // namespace agent
