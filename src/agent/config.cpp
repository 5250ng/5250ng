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
#include "tool_definitions.h"
#include <QSettings>

namespace agent {

static const QString kDefaultSystemPrompt =
    "You are an assistant integrated into a TN5250 terminal emulator (5250ng). "
    "You help users navigate IBM AS/400 (iSeries / IBM i) systems. "
    "When the user sends a message, you will also receive the current screen content "
    "so you can see what they are looking at. Provide concise, helpful answers about "
    "5250 commands, CL commands, RPG, COBOL, DDS, and general IBM i administration.\n\n"
    "You have two tools for terminal automation:\n"
    "- generate_5250script: Generate a script from a task description using a specialized subagent. "
    "Always use this tool to create scripts — the subagent has the complete 5250script language reference.\n"
    "- run_5250script: Execute a 5250script on the terminal. Use this to run scripts returned by generate_5250script.\n\n"
    "Typical flow: call generate_5250script with the task description, then call run_5250script with the generated script. "
    "If a task requires many steps, break it into smaller scripts.";

AgentConfig::AgentConfig()
    : m_activeProviderId("openai"),
      m_openaiModel("gpt-4o"),
      m_anthropicModel("claude-sonnet-4-20250514"),
      m_systemPrompt(kDefaultSystemPrompt) {
    // Default Anthropic OAuth endpoints
    m_anthropicOAuth.authorizationEndpoint = "https://console.anthropic.com/oauth/authorize";
    m_anthropicOAuth.tokenEndpoint = "https://console.anthropic.com/v1/oauth/token";
    m_anthropicOAuth.scope = "user:inference";
    m_anthropicOAuth.providerName = "Claude (Anthropic)";
}

AgentConfig &AgentConfig::instance() {
    static AgentConfig s;
    return s;
}

static AuthType stringToAuthType(const QString &s) {
    if (s == "oauth") return AuthType::OAuth;
    return AuthType::ApiKey;
}

static QString authTypeToString(AuthType t) {
    switch (t) {
    case AuthType::OAuth: return "oauth";
    default: return "apikey";
    }
}

static void loadOAuthConfig(QSettings &settings, const QString &prefix, OAuthConfig &cfg) {
    cfg.authorizationEndpoint = settings.value(prefix + "AuthEndpoint", cfg.authorizationEndpoint).toString();
    cfg.tokenEndpoint = settings.value(prefix + "TokenEndpoint", cfg.tokenEndpoint).toString();
    cfg.clientId = settings.value(prefix + "ClientId", cfg.clientId).toString();
    cfg.scope = settings.value(prefix + "Scope", cfg.scope).toString();
    cfg.providerName = settings.value(prefix + "ProviderName", cfg.providerName).toString();
}

static void saveOAuthConfig(QSettings &settings, const QString &prefix, const OAuthConfig &cfg) {
    settings.setValue(prefix + "AuthEndpoint", cfg.authorizationEndpoint);
    settings.setValue(prefix + "TokenEndpoint", cfg.tokenEndpoint);
    settings.setValue(prefix + "ClientId", cfg.clientId);
    settings.setValue(prefix + "Scope", cfg.scope);
    settings.setValue(prefix + "ProviderName", cfg.providerName);
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

    m_anthropicAuthType = stringToAuthType(settings.value("anthropicAuthType", "apikey").toString());
    m_openaiAuthType = stringToAuthType(settings.value("openaiAuthType", "apikey").toString());

    loadOAuthConfig(settings, "anthropicOAuth", m_anthropicOAuth);
    loadOAuthConfig(settings, "openaiOAuth", m_openaiOAuth);

    m_autoAcceptToolCalls = settings.value("autoAcceptToolCalls", false).toBool();
    m_autoAcceptFileEdits = settings.value("autoAcceptFileEdits", false).toBool();
    m_mcpServerEnabled = settings.value("mcpServerEnabled", false).toBool();
    m_mcpServerPort = static_cast<quint16>(settings.value("mcpServerPort", 9250).toUInt());

    // Per-tool config
    const QStringList toolNames = {
        kToolConnect, kToolGenerateScript, kToolGetCursorPosition,
        kToolGetFieldAt, kToolListFiles, kToolLogin,
        kToolReadFile, kToolReadScreen,
        kToolRunScript, kToolSendKeys, kToolWriteFile};
    settings.beginGroup("tools");
    for (const QString &name : toolNames) {
        settings.beginGroup(name);
        ToolConfig tc;
        tc.enabled = settings.value("enabled", true).toBool();
        tc.customDescription = settings.value("customDescription").toString();
        m_toolConfigs[name] = tc;
        settings.endGroup();
    }
    settings.endGroup();

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

    settings.setValue("anthropicAuthType", authTypeToString(m_anthropicAuthType));
    settings.setValue("openaiAuthType", authTypeToString(m_openaiAuthType));

    saveOAuthConfig(settings, "anthropicOAuth", m_anthropicOAuth);
    saveOAuthConfig(settings, "openaiOAuth", m_openaiOAuth);

    settings.setValue("autoAcceptToolCalls", m_autoAcceptToolCalls);
    settings.setValue("autoAcceptFileEdits", m_autoAcceptFileEdits);
    settings.setValue("mcpServerEnabled", m_mcpServerEnabled);
    settings.setValue("mcpServerPort", m_mcpServerPort);

    // Per-tool config
    settings.beginGroup("tools");
    for (auto it = m_toolConfigs.constBegin(); it != m_toolConfigs.constEnd(); ++it) {
        settings.beginGroup(it.key());
        settings.setValue("enabled", it.value().enabled);
        settings.setValue("customDescription", it.value().customDescription);
        settings.endGroup();
    }
    settings.endGroup();

    settings.endGroup();
}

ToolConfig AgentConfig::toolConfig(const QString &toolName) const {
    return m_toolConfigs.value(toolName);
}

void AgentConfig::setToolConfig(const QString &toolName, const ToolConfig &cfg) {
    m_toolConfigs[toolName] = cfg;
}

} // namespace agent
