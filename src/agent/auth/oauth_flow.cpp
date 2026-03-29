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

#include "oauth_flow.h"
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace agent {

OAuthFlow::OAuthFlow(const OAuthConfig &config, QObject *parent)
    : QObject(parent), m_config(config) {
    connect(&m_server, &QTcpServer::newConnection, this, &OAuthFlow::onNewConnection);
}

OAuthFlow::~OAuthFlow() {
    if (m_server.isListening()) {
        m_server.close();
    }
}

void OAuthFlow::start() {
    m_codeVerifier = generateCodeVerifier();
    m_state = generateState();

    // Bind to loopback only on an ephemeral port
    if (!m_server.listen(QHostAddress::LocalHost, 0)) {
        emit flowFailed("Failed to start local callback server: " + m_server.errorString());
        return;
    }

    quint16 port = m_server.serverPort();
    QString redirectUri = QStringLiteral("http://127.0.0.1:%1/callback").arg(port);

    QUrl authUrl(m_config.authorizationEndpoint);
    QUrlQuery query;
    query.addQueryItem("response_type", "code");
    query.addQueryItem("client_id", m_config.clientId);
    query.addQueryItem("redirect_uri", redirectUri);
    query.addQueryItem("code_challenge", QString::fromLatin1(computeCodeChallenge(m_codeVerifier)));
    query.addQueryItem("code_challenge_method", "S256");
    query.addQueryItem("state", QString::fromLatin1(m_state));
    if (!m_config.scope.isEmpty()) {
        query.addQueryItem("scope", m_config.scope);
    }
    authUrl.setQuery(query);

    if (!QDesktopServices::openUrl(authUrl)) {
        m_server.close();
        emit flowFailed("Failed to open browser for authentication.");
    }
}

void OAuthFlow::abort() {
    m_server.close();
    if (!m_completed) {
        m_completed = true;
        emit flowFailed("OAuth flow was cancelled.");
    }
}

void OAuthFlow::onNewConnection() {
    QTcpSocket *socket = m_server.nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        QByteArray data = socket->readAll();

        // Parse the GET request line to extract query parameters
        int lineEnd = data.indexOf("\r\n");
        if (lineEnd < 0) lineEnd = data.indexOf("\n");
        QByteArray requestLine = (lineEnd > 0) ? data.left(lineEnd) : data;

        // Extract path+query from "GET /callback?code=...&state=... HTTP/1.1"
        int pathStart = requestLine.indexOf(' ') + 1;
        int pathEnd = requestLine.indexOf(' ', pathStart);
        QByteArray pathAndQuery = requestLine.mid(pathStart, pathEnd - pathStart);

        QUrl requestUrl("http://localhost" + QString::fromUtf8(pathAndQuery));
        QUrlQuery query(requestUrl);

        // Send a response to the browser
        QByteArray response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n\r\n"
            "<html><body><h2>Authentication successful</h2>"
            "<p>You can close this tab and return to 5250ng.</p>"
            "</body></html>";
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();

        m_server.close();

        if (m_completed) return;

        // Verify state
        QString receivedState = query.queryItemValue("state");
        if (receivedState.toUtf8() != m_state) {
            m_completed = true;
            emit flowFailed("OAuth state mismatch - possible CSRF attack.");
            return;
        }

        // Check for error
        if (query.hasQueryItem("error")) {
            m_completed = true;
            QString errorDesc = query.queryItemValue("error_description");
            if (errorDesc.isEmpty()) errorDesc = query.queryItemValue("error");
            emit flowFailed("Authorization denied: " + errorDesc);
            return;
        }

        QString code = query.queryItemValue("code");
        if (code.isEmpty()) {
            m_completed = true;
            emit flowFailed("No authorization code received.");
            return;
        }

        exchangeCode(code);
    });
}

void OAuthFlow::exchangeCode(const QString &code) {
    // Server may already be closed, but we stored the port from the redirect_uri
    // Reconstruct the redirect_uri used during authorization
    // Note: we need the same redirect_uri that was used in the authorization request
    // The server was on an ephemeral port, so we reconstruct it from the URL the browser hit.
    // Since the server is closed now, we just build the same URI string.

    QUrl tokenUrl(m_config.tokenEndpoint);
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("grant_type", "authorization_code");
    params.addQueryItem("code", code);
    params.addQueryItem("client_id", m_config.clientId);
    params.addQueryItem("code_verifier", QString::fromLatin1(m_codeVerifier));

    QNetworkReply *reply = m_nam.post(request, params.query(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (m_completed) return;
        m_completed = true;

        if (reply->error() != QNetworkReply::NoError) {
            QByteArray body = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(body);
            QString detail;
            if (doc.isObject()) {
                detail = doc.object()["error_description"].toString();
                if (detail.isEmpty()) detail = doc.object()["error"].toString();
            }
            if (detail.isEmpty()) detail = reply->errorString();
            emit flowFailed("Token exchange failed: " + detail);
            return;
        }

        QByteArray body = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit flowFailed("Invalid token response.");
            return;
        }

        QJsonObject obj = doc.object();
        QString accessToken = obj["access_token"].toString();
        QString refreshToken = obj["refresh_token"].toString();
        int expiresIn = obj["expires_in"].toInt(3600);

        if (accessToken.isEmpty()) {
            emit flowFailed("No access token in response.");
            return;
        }

        emit flowCompleted(accessToken, refreshToken, expiresIn);
    });
}

QByteArray OAuthFlow::generateCodeVerifier() {
    // RFC 7636: 43-128 characters from [A-Z][a-z][0-9]-._~
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    constexpr int len = 64;
    QByteArray verifier(len, '\0');
    auto *rng = QRandomGenerator::system();
    for (int i = 0; i < len; ++i) {
        verifier[i] = charset[rng->bounded(static_cast<int>(sizeof(charset) - 1))];
    }
    return verifier;
}

QByteArray OAuthFlow::computeCodeChallenge(const QByteArray &verifier) {
    // code_challenge = BASE64URL(SHA256(code_verifier))
    QByteArray hash = QCryptographicHash::hash(verifier, QCryptographicHash::Sha256);
    return hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray OAuthFlow::generateState() {
    QByteArray bytes(32, '\0');
    auto *rng = QRandomGenerator::system();
    for (int i = 0; i < 32; ++i) {
        bytes[i] = static_cast<char>(rng->bounded(256));
    }
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

} // namespace agent
