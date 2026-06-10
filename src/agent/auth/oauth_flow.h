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

#include "oauth_config.h"
#include <QNetworkAccessManager>
#include <QObject>
#include <QTcpServer>

namespace agent {

/// Performs a single OAuth2 Authorization Code + PKCE flow.
///
/// 1. Generates code_verifier / code_challenge (S256)
/// 2. Starts a local loopback HTTP server for the redirect
/// 3. Opens the user's browser to the authorization endpoint
/// 4. Receives the authorization code via redirect callback
/// 5. Exchanges the code for access/refresh tokens
/// 6. Emits flowCompleted or flowFailed, then self-destructs
class OAuthFlow : public QObject {
    Q_OBJECT

  public:
    explicit OAuthFlow(const OAuthConfig &config, QObject *parent = nullptr);
    ~OAuthFlow() override;

    /// Start the OAuth flow. Opens the user's default browser.
    void start();

    /// Abort the flow if still in progress.
    void abort();

  signals:
    void flowCompleted(const QString &accessToken, const QString &refreshToken, int expiresIn);
    void flowFailed(const QString &error);

  private:
    void onNewConnection();
    void exchangeCode(const QString &code);

    static QByteArray generateCodeVerifier();
    static QByteArray computeCodeChallenge(const QByteArray &verifier);
    static QByteArray generateState();

    OAuthConfig m_config;
    QTcpServer m_server;
    QNetworkAccessManager m_nam;
    QByteArray m_codeVerifier;
    QByteArray m_state;
    // Redirect URI sent in the authorization request. RFC 6749 §4.1.3
    // requires the token request to repeat the identical value, and the
    // callback server may already be closed by then, so it is kept here.
    QString m_redirectUri;
    bool m_completed = false;
};

} // namespace agent
