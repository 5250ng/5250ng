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

#include "agent/provider.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace agent {

class OpenAiProvider : public Provider {
    Q_OBJECT

  public:
    explicit OpenAiProvider(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("openai"); }
    QString displayName() const override { return QStringLiteral("OpenAI"); }
    QStringList availableModels() const override;
    void sendMessage(const QString &userMessage, const QString &systemContext) override;
    void cancel() override;
    bool isConfigured() const override;

    void setApiKey(const QString &key) override { m_apiKey = key; }
    void setModel(const QString &model) override { m_model = model; }
    QString apiKey() const override { return m_apiKey; }
    QString model() const override { return m_model; }

  private:
    QNetworkAccessManager m_nam;
    QNetworkReply *m_activeReply = nullptr;
    QString m_apiKey;
    QString m_model;
};

} // namespace agent
