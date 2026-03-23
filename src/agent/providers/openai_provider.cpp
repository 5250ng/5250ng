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

#include "openai_provider.h"
#include "agent/auth/auth_method.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

namespace agent {

OpenAiProvider::OpenAiProvider(QObject *parent) : Provider(parent) {}

QStringList OpenAiProvider::availableModels() const {
    return {
        "gpt-4o",
        "gpt-4o-mini",
        "gpt-4-turbo",
        "gpt-3.5-turbo",
    };
}

bool OpenAiProvider::isConfigured() const {
    if (m_authMethod && m_authMethod->isAuthenticated() && !m_model.isEmpty())
        return true;
    return !m_apiKey.isEmpty() && !m_model.isEmpty();
}

void OpenAiProvider::sendMessage(const QString &userMessage, const QString &systemContext) {
    if (!isConfigured()) {
        emit responseError("OpenAI provider is not configured. Please set an API key and model in Settings → Agent.");
        return;
    }

    // Cancel any in-flight request before starting a new one
    cancel();

    QJsonArray messages;
    if (!systemContext.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = systemContext;
        messages.append(sysMsg);
    }
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);

    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = messages;

    QNetworkRequest request(QUrl("https://api.openai.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (m_authMethod) {
        m_authMethod->applyAuth(request);
    } else {
        request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
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
            emit responseError("OpenAI error: " + detail);
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        if (!doc.isObject()) {
            emit responseError("Invalid response from OpenAI.");
            return;
        }

        QJsonArray choices = doc.object()["choices"].toArray();
        if (choices.isEmpty()) {
            emit responseError("No response from OpenAI.");
            return;
        }

        QString content = choices[0].toObject()["message"].toObject()["content"].toString();
        emit responseReceived(content);
    });
}

void OpenAiProvider::cancel() {
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

} // namespace agent
