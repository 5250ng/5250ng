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
    m_redirectUri = QStringLiteral("http://127.0.0.1:%1/callback").arg(port);

    QUrl authUrl(m_config.authorizationEndpoint);
    QUrlQuery query;
    query.addQueryItem("response_type", "code");
    query.addQueryItem("client_id", m_config.clientId);
    query.addQueryItem("redirect_uri", m_redirectUri);
    query.addQueryItem("code_challenge", QString::fromLatin1(computeCodeChallenge(m_codeVerifier)));
    query.addQueryItem("code_challenge_method", "S256");
    query.addQueryItem("state", QString::fromLatin1(m_state));
    if (!m_config.scope.isEmpty()) {
        query.addQueryItem("scope", m_config.scope);
    }
    authUrl.setQuery(query);

    const bool opened = m_openUrl ? m_openUrl(authUrl) : QDesktopServices::openUrl(authUrl);
    if (!opened) {
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

namespace {

void sendPage(QTcpSocket *socket, const QByteArray &status, const QString &title,
              const QString &text) {
    const QByteArray body = QStringLiteral("<html><body><h2>%1</h2><p>%2</p></body></html>")
                                .arg(title.toHtmlEscaped(), text.toHtmlEscaped())
                                .toUtf8();
    socket->write("HTTP/1.1 " + status + "\r\n"
                  "Content-Type: text/html; charset=utf-8\r\n"
                  "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                  "Connection: close\r\n\r\n" + body);
    socket->flush();
    socket->disconnectFromHost();
}

} // namespace

void OAuthFlow::onNewConnection() {
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        // A request can arrive in several reads: wait for the full request
        // line before parsing it.
        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
            buffer->append(socket->readAll());
            int lineEnd = buffer->indexOf("\r\n");
            if (lineEnd < 0) lineEnd = buffer->indexOf('\n');
            if (lineEnd < 0) {
                if (buffer->size() > 8192)
                    sendPage(socket, "414 URI Too Long", "Request too long", QString());
                return;
            }
            const QByteArray requestLine = buffer->left(lineEnd);
            buffer->clear();
            disconnect(socket, &QTcpSocket::readyRead, this, nullptr);

            // "GET /callback?code=...&state=... HTTP/1.1"
            const QList<QByteArray> parts = requestLine.split(' ');
            const QUrl requestUrl(QStringLiteral("http://localhost")
                                  + QString::fromUtf8(parts.value(1)));
            const QUrlQuery query(requestUrl);

            // Anything else the browser asks for (favicon.ico, ...) is not
            // the callback: answer it and keep waiting.
            if (parts.value(0) != "GET" || requestUrl.path() != QLatin1String("/callback")) {
                sendPage(socket, "404 Not Found", "Not found", QString());
                return;
            }
            if (m_callbackHandled || m_completed) {
                sendPage(socket, "410 Gone", "Sign-in already completed",
                         "You can close this tab and return to 5250ng.");
                return;
            }
            // A callback without our state did not come from the sign-in we
            // started (CSRF, or a stale tab): refuse it, keep waiting for
            // the real one.
            if (query.queryItemValue("state").toUtf8() != m_state) {
                sendPage(socket, "400 Bad Request", "Authentication failed",
                         "The sign-in response did not match this request.");
                return;
            }

            m_callbackHandled = true;
            m_server.close();

            if (query.hasQueryItem("error")) {
                m_completed = true;
                QString errorDesc = query.queryItemValue("error_description", QUrl::FullyDecoded);
                if (errorDesc.isEmpty()) errorDesc = query.queryItemValue("error");
                sendPage(socket, "200 OK", "Authentication failed", "Authorization denied: " + errorDesc);
                emit flowFailed("Authorization denied: " + errorDesc);
                return;
            }

            const QString code = query.queryItemValue("code");
            if (code.isEmpty()) {
                m_completed = true;
                sendPage(socket, "200 OK", "Authentication failed", "No authorization code received.");
                emit flowFailed("No authorization code received.");
                return;
            }

            sendPage(socket, "200 OK", "Authorization received",
                     "You can close this tab and return to 5250ng.");
            exchangeCode(code);
        });
    }
}

void OAuthFlow::exchangeCode(const QString &code) {
    QUrl tokenUrl(m_config.tokenEndpoint);
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("grant_type", "authorization_code");
    params.addQueryItem("code", code);
    params.addQueryItem("client_id", m_config.clientId);
    // RFC 6749 §4.1.3: redirect_uri is REQUIRED here (and must be identical)
    // because the authorization request included it. The callback server may
    // already be closed, so the value was saved in start().
    params.addQueryItem("redirect_uri", m_redirectUri);
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
