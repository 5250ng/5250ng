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
#include "agent/tool_definitions.h"
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
        emit responseError("OpenAI provider is not configured. Please set an API key and model in Settings \u2192 Agent.");
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

void OpenAiProvider::sendToolResult(const ToolResult &result) {
    // Append the assistant message that contained the tool_calls
    if (!m_pendingAssistantMessage.isEmpty()) {
        m_conversationHistory.append(m_pendingAssistantMessage);
        m_pendingAssistantMessage = QJsonObject();
    }

    // Append tool result message
    QJsonObject toolMsg;
    toolMsg["role"] = "tool";
    toolMsg["tool_call_id"] = result.toolCallId;
    toolMsg["content"] = result.success
        ? result.output
        : QStringLiteral("Error: %1").arg(result.output);
    m_conversationHistory.append(toolMsg);

    sendRequest();
}

void OpenAiProvider::clearHistory() {
    m_conversationHistory = QJsonArray();
    m_pendingAssistantMessage = QJsonObject();
}

void OpenAiProvider::sendRequest() {
    cancel();

    // Build messages array: system + conversation history
    QJsonArray messages;
    if (!m_systemContext.isEmpty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = m_systemContext;
        messages.append(sysMsg);
    }
    for (const QJsonValue &msg : m_conversationHistory) {
        messages.append(msg);
    }

    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = messages;
    body["tools"] = openaiToolsArray();
    // This client executes exactly one tool call per turn (handleResponse
    // dispatches a single ToolCall and sendToolResult appends a single
    // role:"tool" message). A turn with N>1 tool_calls would require N tool
    // messages in the continuation or the API rejects it with a 400 — so
    // tell the API not to emit parallel tool calls.
    body["parallel_tool_calls"] = false;

    QNetworkRequest request(QUrl("https://api.openai.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (m_authMethod) {
        m_authMethod->applyAuth(request);
    } else {
        request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
    }

    m_activeReply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
        handleResponse(m_activeReply);
    });
}

void OpenAiProvider::handleResponse(QNetworkReply *reply) {
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

    QJsonObject choice = choices[0].toObject();
    QJsonObject message = choice["message"].toObject();
    QString finishReason = choice["finish_reason"].toString();

    // Check for tool calls
    QJsonArray toolCalls = message["tool_calls"].toArray();
    if (!toolCalls.isEmpty() && finishReason == "tool_calls") {
        // Save the full assistant message for continuation
        m_pendingAssistantMessage = message;

        QJsonObject tc = toolCalls[0].toObject();
        QJsonObject function = tc["function"].toObject();

        ToolCall toolCall;
        toolCall.id = tc["id"].toString();
        toolCall.name = function["name"].toString();
        toolCall.arguments = function["arguments"].toString();
        // OpenAI may include text content alongside tool calls
        toolCall.textBefore = message["content"].toString();

        emit toolCallReceived(toolCall);
    } else {
        // Regular text response — append to history
        m_conversationHistory.append(message);

        QString content = message["content"].toString();
        if (content.isEmpty()) {
            emit responseError("No response from OpenAI.");
        } else {
            emit responseReceived(content);
        }
    }
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
