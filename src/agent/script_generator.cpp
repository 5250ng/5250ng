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

#include "script_generator.h"
#include "auth/auth_method.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

namespace agent {

ScriptGeneratorSubagent::ScriptGeneratorSubagent(QObject *parent)
    : QObject(parent) {}

void ScriptGeneratorSubagent::configure(const QString &providerId,
                                        const QString &model,
                                        const QString &apiKey,
                                        AuthMethod *authMethod) {
    m_providerId = providerId;
    m_model = model;
    m_apiKey = apiKey;
    m_authMethod = authMethod;
}

void ScriptGeneratorSubagent::generate(const QString &task,
                                       const QString &screenContext) {
    cancel();

    QString promptTemplate = loadPromptTemplate();
    if (promptTemplate.isEmpty()) {
        emit generationError("Could not load 5250script prompt template.");
        return;
    }

    QString userMessage = QStringLiteral("Task: %1").arg(task);
    if (!screenContext.isEmpty()) {
        userMessage += QStringLiteral(
            "\n\nCurrent terminal screen:\n%1").arg(screenContext);
    }
    userMessage += QStringLiteral(
        "\n\nGenerate a 5250script that accomplishes this task. "
        "Output ONLY the script code, no explanations.");

    if (m_providerId == "anthropic") {
        sendAnthropicRequest(promptTemplate, userMessage);
    } else {
        sendOpenAiRequest(promptTemplate, userMessage);
    }
}

void ScriptGeneratorSubagent::cancel() {
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Anthropic
// ---------------------------------------------------------------------------

void ScriptGeneratorSubagent::sendAnthropicRequest(
    const QString &systemPrompt, const QString &userMessage) {

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);

    QJsonObject body;
    body["model"] = m_model;
    body["max_tokens"] = 4096;
    body["messages"] = messages;
    body["system"] = systemPrompt;

    QNetworkRequest request(
        QUrl("https://api.anthropic.com/v1/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("anthropic-version", "2023-06-01");
    if (m_authMethod) {
        m_authMethod->applyAuth(request);
    } else {
        request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    }

    m_activeReply = m_nam.post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_activeReply;
        m_activeReply = nullptr;
        if (!reply) return;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QString detail;
            if (doc.isObject() && doc.object().contains("error"))
                detail = doc.object()["error"].toObject()["message"].toString();
            if (detail.isEmpty())
                detail = reply->errorString();
            emit generationError("Anthropic error: " + detail);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            emit generationError("Invalid response from Anthropic subagent.");
            return;
        }

        QJsonArray content = doc.object()["content"].toArray();
        QString text;
        for (const QJsonValue &block : content) {
            if (block.toObject()["type"].toString() == "text") {
                if (!text.isEmpty()) text += '\n';
                text += block.toObject()["text"].toString();
            }
        }

        if (text.isEmpty()) {
            emit generationError("Empty response from Anthropic subagent.");
            return;
        }

        emit scriptGenerated(stripCodeFences(text));
    });
}

// ---------------------------------------------------------------------------
// OpenAI
// ---------------------------------------------------------------------------

void ScriptGeneratorSubagent::sendOpenAiRequest(
    const QString &systemPrompt, const QString &userMessage) {

    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = systemPrompt;
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    messages.append(userMsg);

    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = messages;

    QNetworkRequest request(
        QUrl("https://api.openai.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (m_authMethod) {
        m_authMethod->applyAuth(request);
    } else {
        request.setRawHeader("Authorization",
                             ("Bearer " + m_apiKey).toUtf8());
    }

    m_activeReply = m_nam.post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_activeReply;
        m_activeReply = nullptr;
        if (!reply) return;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QString detail;
            if (doc.isObject() && doc.object().contains("error"))
                detail = doc.object()["error"].toObject()["message"].toString();
            if (detail.isEmpty())
                detail = reply->errorString();
            emit generationError("OpenAI error: " + detail);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            emit generationError("Invalid response from OpenAI subagent.");
            return;
        }

        QJsonArray choices = doc.object()["choices"].toArray();
        if (choices.isEmpty()) {
            emit generationError("Empty response from OpenAI subagent.");
            return;
        }

        QString text = choices[0].toObject()["message"]
                           .toObject()["content"].toString();
        if (text.isEmpty()) {
            emit generationError("Empty content from OpenAI subagent.");
            return;
        }

        emit scriptGenerated(stripCodeFences(text));
    });
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString ScriptGeneratorSubagent::loadPromptTemplate() {
    QFile f(":/agent/5250script_prompt.md");
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

QString ScriptGeneratorSubagent::stripCodeFences(const QString &text) {
    QString result = text.trimmed();
    // Remove leading ```... line
    if (result.startsWith("```")) {
        int newline = result.indexOf('\n');
        if (newline >= 0)
            result = result.mid(newline + 1);
    }
    // Remove trailing ``` line
    if (result.endsWith("```")) {
        int lastNewline = result.lastIndexOf('\n');
        if (lastNewline >= 0)
            result = result.left(lastNewline);
    }
    return result.trimmed();
}

} // namespace agent
