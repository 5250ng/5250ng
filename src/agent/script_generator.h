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

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

namespace agent {

class AuthMethod;

/// Makes a one-shot LLM call to generate a 5250script from a task description.
/// Uses the 5250script language reference (PROMPT.md) as the system prompt.
class ScriptGeneratorSubagent : public QObject {
    Q_OBJECT

  public:
    explicit ScriptGeneratorSubagent(QObject *parent = nullptr);

    /// Configure with the same credentials as the main provider.
    /// @param authMethod Non-owning pointer; must remain valid during generate().
    void configure(const QString &providerId,
                   const QString &model,
                   const QString &apiKey,
                   AuthMethod *authMethod);

    /// Generate a script for the given task with current screen context.
    void generate(const QString &task, const QString &screenContext);

    void cancel();

  signals:
    void scriptGenerated(const QString &script);
    void generationError(const QString &error);

  private:
    void sendAnthropicRequest(const QString &systemPrompt,
                              const QString &userMessage);
    void sendOpenAiRequest(const QString &systemPrompt,
                           const QString &userMessage);

    static QString loadPromptTemplate();
    static QString stripCodeFences(const QString &text);

    QNetworkAccessManager m_nam;
    QNetworkReply *m_activeReply = nullptr;
    QString m_providerId;
    QString m_model;
    QString m_apiKey;
    AuthMethod *m_authMethod = nullptr;
};

} // namespace agent
