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

#include "oauth_auth.h"
#include "oauth_flow.h"
#include "token_storage.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

namespace agent {

OAuthAuth::OAuthAuth(const OAuthConfig &config, const QString &providerId, QObject *parent)
    : AuthMethod(parent), m_config(config), m_providerId(providerId) {
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &OAuthAuth::refreshAccessToken);
}

bool OAuthAuth::isAuthenticated() const {
    return !m_accessToken.isEmpty() && QDateTime::currentDateTimeUtc() < m_expiresAt;
}

void OAuthAuth::applyAuth(QNetworkRequest &request) {
    if (!m_accessToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    }
}

void OAuthAuth::authenticate() {
    auto *flow = new OAuthFlow(m_config, this);

    connect(flow, &OAuthFlow::flowCompleted, this,
            [this, flow](const QString &accessToken, const QString &refreshToken, int expiresIn) {
                onTokensReceived(accessToken, refreshToken, expiresIn);
                emit authenticationSucceeded();
                flow->deleteLater();
            });

    connect(flow, &OAuthFlow::flowFailed, this,
            [this, flow](const QString &error) {
                emit authenticationFailed(error);
                flow->deleteLater();
            });

    flow->start();
}

void OAuthAuth::logout() {
    m_accessToken.clear();
    m_refreshToken.clear();
    m_expiresAt = QDateTime();
    m_refreshTimer.stop();
    TokenStorage::instance().clearTokens(m_providerId);
}

void OAuthAuth::loadFromStorage() {
    TokenStorage::instance().loadTokens(m_providerId, m_accessToken, m_refreshToken, m_expiresAt);
    if (!m_accessToken.isEmpty()) {
        scheduleRefresh();
    }
}

void OAuthAuth::refreshAccessToken() {
    if (m_refreshToken.isEmpty()) {
        emit authenticationFailed("No refresh token available. Please sign in again.");
        return;
    }

    QUrl tokenUrl(m_config.tokenEndpoint);
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("grant_type", "refresh_token");
    params.addQueryItem("refresh_token", m_refreshToken);
    params.addQueryItem("client_id", m_config.clientId);

    QNetworkReply *reply = m_nam.post(request, params.query(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QByteArray body = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(body);
            QString detail;
            if (doc.isObject()) {
                detail = doc.object()["error_description"].toString();
                if (detail.isEmpty()) detail = doc.object()["error"].toString();
            }
            if (detail.isEmpty()) detail = reply->errorString();
            emit authenticationFailed("Token refresh failed: " + detail);
            return;
        }

        QByteArray body = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit authenticationFailed("Invalid refresh response.");
            return;
        }

        QJsonObject obj = doc.object();
        QString accessToken = obj["access_token"].toString();
        QString refreshToken = obj["refresh_token"].toString(m_refreshToken);
        int expiresIn = obj["expires_in"].toInt(3600);

        if (accessToken.isEmpty()) {
            emit authenticationFailed("No access token in refresh response.");
            return;
        }

        onTokensReceived(accessToken, refreshToken, expiresIn);
        emit tokenRefreshed();
    });
}

void OAuthAuth::onTokensReceived(const QString &accessToken, const QString &refreshToken,
                                 int expiresIn) {
    m_accessToken = accessToken;
    m_refreshToken = refreshToken;
    m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
    TokenStorage::instance().saveTokens(m_providerId, m_accessToken, m_refreshToken, m_expiresAt);
    scheduleRefresh();
}

void OAuthAuth::scheduleRefresh() {
    if (m_refreshToken.isEmpty()) return;

    // Refresh 60 seconds before expiry, minimum 10 seconds from now
    qint64 msUntilExpiry = QDateTime::currentDateTimeUtc().msecsTo(m_expiresAt);
    qint64 refreshIn = qMax(msUntilExpiry - 60000, qint64(10000));
    m_refreshTimer.start(static_cast<int>(qMin(refreshIn, qint64(INT_MAX))));
}

} // namespace agent
