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

#include "tool_call.h"
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>

namespace agent {

class AuthMethod;

class Provider : public QObject {
    Q_OBJECT

  public:
    explicit Provider(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~Provider() = default;

    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual QStringList availableModels() const = 0;
    virtual void sendMessage(const QString &userMessage, const QString &systemContext) = 0;
    virtual void sendToolResult(const ToolResult &result) = 0;
    virtual void clearHistory() = 0;
    virtual void cancel() = 0;
    virtual bool isConfigured() const = 0;

    virtual void setApiKey(const QString &key) = 0;
    virtual void setModel(const QString &model) = 0;
    virtual QString apiKey() const = 0;
    virtual QString model() const = 0;

    /// Set an authentication method. Takes ownership.
    /// When set, providers should delegate auth header logic to this object.
    void setAuthMethod(AuthMethod *auth);
    AuthMethod *authMethod() const { return m_authMethod; }

  signals:
    void responseReceived(const QString &text);
    void responseError(const QString &error);
    void streamingChunk(const QString &chunk);
    void toolCallReceived(const agent::ToolCall &call);

  protected:
    AuthMethod *m_authMethod = nullptr;
    QJsonArray m_conversationHistory;
};

} // namespace agent
