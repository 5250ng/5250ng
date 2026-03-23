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

#include "anthropic_provider.h"
#include "agent/auth/auth_method.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

namespace agent {

AnthropicProvider::AnthropicProvider(QObject *parent) : Provider(parent) {}

QStringList AnthropicProvider::availableModels() const {
    return {
        "claude-sonnet-4-20250514",
        "claude-haiku-4-5-20251001",
    };
}

bool AnthropicProvider::isConfigured() const {
    if (m_authMethod && m_authMethod->isAuthenticated() && !m_model.isEmpty())
        return true;
    return !m_apiKey.isEmpty() && !m_model.isEmpty();
}

void AnthropicProvider::sendMessage(const QString &userMessage, const QString &systemContext) {
    if (!isConfigured()) {
        emit responseError("Anthropic provider is not configured. Please set an API key and model in Settings → Agent.");
        return;
    }

    // Cancel any in-flight request before starting a new one
    cancel();

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);

    QJsonObject body;
    body["model"] = m_model;
    body["max_tokens"] = 4096;
    body["messages"] = messages;

    if (!systemContext.isEmpty()) {
        body["system"] = systemContext;
    }

    QNetworkRequest request(QUrl("https://api.anthropic.com/v1/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("anthropic-version", "2023-06-01");
    if (m_authMethod) {
        m_authMethod->applyAuth(request);
    } else {
        request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    }

    m_activeReply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_activeReply;
        m_activeReply = nullptr;

        if (!reply) return;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            QString detail;
            if (doc.isObject() && doc.object().contains("error")) {
                detail = doc.object()["error"].toObject()["message"].toString();
            }
            if (detail.isEmpty()) {
                detail = reply->errorString();
            }
            emit responseError("Anthropic error: " + detail);
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        if (!doc.isObject()) {
            emit responseError("Invalid response from Anthropic.");
            return;
        }

        QJsonArray content = doc.object()["content"].toArray();
        if (content.isEmpty()) {
            emit responseError("No response from Anthropic.");
            return;
        }

        QString text = content[0].toObject()["text"].toString();
        emit responseReceived(text);
    });
}

void AnthropicProvider::cancel() {
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

} // namespace agent
