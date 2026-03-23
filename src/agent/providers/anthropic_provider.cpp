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
#include "agent/tool_definitions.h"
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
        emit responseError("Anthropic provider is not configured. Please set an API key and model in Settings \u2192 Agent.");
        return;
    }

    cancel();
    m_systemContext = systemContext;

    // Append user message to conversation history
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;
    m_conversationHistory.append(userMsg);

    sendRequest();
}

void AnthropicProvider::sendToolResult(const ToolResult &result) {
    // Append the assistant message that contained the tool_use
    if (!m_pendingAssistantMessage.isEmpty()) {
        m_conversationHistory.append(m_pendingAssistantMessage);
        m_pendingAssistantMessage = QJsonObject();
    }

    // Append tool result as a user message
    QJsonObject toolResultBlock;
    toolResultBlock["type"] = "tool_result";
    toolResultBlock["tool_use_id"] = result.toolCallId;
    toolResultBlock["content"] = result.success
        ? result.output
        : QStringLiteral("Error: %1").arg(result.output);

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = QJsonArray{toolResultBlock};
    m_conversationHistory.append(userMsg);

    sendRequest();
}

void AnthropicProvider::clearHistory() {
    m_conversationHistory = QJsonArray();
    m_pendingAssistantMessage = QJsonObject();
}

void AnthropicProvider::sendRequest() {
    cancel();

    QJsonObject body;
    body["model"] = m_model;
    body["max_tokens"] = 4096;
    body["messages"] = m_conversationHistory;
    body["tools"] = anthropicToolsArray();

    if (!m_systemContext.isEmpty()) {
        body["system"] = m_systemContext;
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
        handleResponse(m_activeReply);
    });
}

void AnthropicProvider::handleResponse(QNetworkReply *reply) {
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

    QJsonObject root = doc.object();
    QJsonArray content = root["content"].toArray();
    QString stopReason = root["stop_reason"].toString();

    // Collect text blocks and find tool_use blocks
    QString combinedText;
    ToolCall toolCall;
    bool hasToolUse = false;

    for (const QJsonValue &block : content) {
        QJsonObject obj = block.toObject();
        QString type = obj["type"].toString();

        if (type == "text") {
            if (!combinedText.isEmpty())
                combinedText += "\n";
            combinedText += obj["text"].toString();
        } else if (type == "tool_use") {
            hasToolUse = true;
            toolCall.id = obj["id"].toString();
            toolCall.name = obj["name"].toString();
            toolCall.arguments = QString::fromUtf8(
                QJsonDocument(obj["input"].toObject()).toJson(QJsonDocument::Compact));
            toolCall.textBefore = combinedText;
        }
    }

    if (hasToolUse && stopReason == "tool_use") {
        // Save the full assistant message for the continuation
        QJsonObject assistantMsg;
        assistantMsg["role"] = "assistant";
        assistantMsg["content"] = content;
        m_pendingAssistantMessage = assistantMsg;

        emit toolCallReceived(toolCall);
    } else {
        // Regular text response — append to history
        QJsonObject assistantMsg;
        assistantMsg["role"] = "assistant";
        assistantMsg["content"] = content;
        m_conversationHistory.append(assistantMsg);

        if (combinedText.isEmpty()) {
            emit responseError("No response from Anthropic.");
        } else {
            emit responseReceived(combinedText);
        }
    }
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
