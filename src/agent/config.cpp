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

#include "config.h"
#include <QSettings>

namespace agent {

static const QString kDefaultSystemPrompt =
    "You are an assistant integrated into a TN5250 terminal emulator (5250ng). "
    "You help users navigate IBM AS/400 (iSeries / IBM i) systems. "
    "When the user sends a message, you will also receive the current screen content "
    "so you can see what they are looking at. Provide concise, helpful answers about "
    "5250 commands, CL commands, RPG, COBOL, DDS, and general IBM i administration.";

AgentConfig::AgentConfig()
    : m_activeProviderId("openai"),
      m_openaiModel("gpt-4o"),
      m_anthropicModel("claude-sonnet-4-20250514"),
      m_systemPrompt(kDefaultSystemPrompt) {}

AgentConfig &AgentConfig::instance() {
    static AgentConfig s;
    return s;
}

void AgentConfig::load() {
    QSettings settings;
    settings.beginGroup("AI");
    m_activeProviderId = settings.value("activeProvider", m_activeProviderId).toString();
    m_openaiApiKey = settings.value("openaiApiKey").toString();
    m_openaiModel = settings.value("openaiModel", m_openaiModel).toString();
    m_anthropicApiKey = settings.value("anthropicApiKey").toString();
    m_anthropicModel = settings.value("anthropicModel", m_anthropicModel).toString();
    m_systemPrompt = settings.value("systemPrompt", m_systemPrompt).toString();
    settings.endGroup();
}

void AgentConfig::save() {
    QSettings settings;
    settings.beginGroup("AI");
    settings.setValue("activeProvider", m_activeProviderId);
    settings.setValue("openaiApiKey", m_openaiApiKey);
    settings.setValue("openaiModel", m_openaiModel);
    settings.setValue("anthropicApiKey", m_anthropicApiKey);
    settings.setValue("anthropicModel", m_anthropicModel);
    settings.setValue("systemPrompt", m_systemPrompt);
    settings.endGroup();
}

} // namespace agent
