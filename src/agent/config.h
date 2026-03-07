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

#include <QString>

namespace agent {

class AgentConfig {
  public:
    static AgentConfig &instance();

    void load();
    void save();

    QString activeProviderId() const { return m_activeProviderId; }
    void setActiveProviderId(const QString &id) { m_activeProviderId = id; }

    QString openaiApiKey() const { return m_openaiApiKey; }
    void setOpenaiApiKey(const QString &key) { m_openaiApiKey = key; }

    QString openaiModel() const { return m_openaiModel; }
    void setOpenaiModel(const QString &model) { m_openaiModel = model; }

    QString anthropicApiKey() const { return m_anthropicApiKey; }
    void setAnthropicApiKey(const QString &key) { m_anthropicApiKey = key; }

    QString anthropicModel() const { return m_anthropicModel; }
    void setAnthropicModel(const QString &model) { m_anthropicModel = model; }


    QString systemPrompt() const { return m_systemPrompt; }
    void setSystemPrompt(const QString &prompt) { m_systemPrompt = prompt; }

  private:
    AgentConfig();

    QString m_activeProviderId;
    QString m_openaiApiKey;
    QString m_openaiModel;
    QString m_anthropicApiKey;
    QString m_anthropicModel;
    QString m_systemPrompt;
};

} // namespace agent
