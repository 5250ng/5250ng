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

#include "session/config.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace session {

SessionConfig::SessionConfig(QObject *parent) : QObject(parent), m_name("New Session"), m_hostname(""), m_port(23), m_useTLS(false), m_deviceType("IBM-3179-2"), m_deviceName(""), m_screenRows(24), m_screenCols(80), m_codePage(core::CodePage::ID::CP037), m_terminalThemeId("classic_green") {}

SessionConfig::SessionConfig(const SessionConfig &other) : QObject(other.parent()), m_name(other.m_name), m_hostname(other.m_hostname), m_port(other.m_port), m_useTLS(other.m_useTLS), m_allowInvalidCertificates(other.m_allowInvalidCertificates), m_deviceType(other.m_deviceType), m_deviceName(other.m_deviceName), m_screenRows(other.m_screenRows), m_screenCols(other.m_screenCols), m_codePage(other.m_codePage), m_terminalThemeId(other.m_terminalThemeId), m_startupScriptSource(other.m_startupScriptSource), m_startupScriptName(other.m_startupScriptName), m_sessionVariables(other.m_sessionVariables), m_username(other.m_username), m_password(other.m_password), m_pcCommandPolicy(other.m_pcCommandPolicy) {}

SessionConfig &SessionConfig::operator=(const SessionConfig &other) {
    if (this != &other) {
        m_name = other.m_name;
        m_hostname = other.m_hostname;
        m_port = other.m_port;
        m_useTLS = other.m_useTLS;
        m_allowInvalidCertificates = other.m_allowInvalidCertificates;
        m_deviceType = other.m_deviceType;
        m_deviceName = other.m_deviceName;
        m_screenRows = other.m_screenRows;
        m_screenCols = other.m_screenCols;
        m_codePage = other.m_codePage;
        m_terminalThemeId = other.m_terminalThemeId;
        m_startupScriptSource = other.m_startupScriptSource;
        m_startupScriptName = other.m_startupScriptName;
        m_sessionVariables = other.m_sessionVariables;
        m_username = other.m_username;
        m_password = other.m_password;
        m_pcCommandPolicy = other.m_pcCommandPolicy;
        emit changed();
    }
    return *this;
}

namespace {

// String tags persisted in JSON for each PcCommandPolicy. Strings are stable
// over time and self-documenting in saved config files; an int would be
// shorter but harder to grep and easy to misread when comparing files.
QString pcCommandPolicyToString(PcCommandPolicy policy) {
    switch (policy) {
    case PcCommandPolicy::Deny:            return QStringLiteral("deny");
    case PcCommandPolicy::DenyAndAlert:    return QStringLiteral("denyAlert");
    case PcCommandPolicy::AllowWithPrompt: return QStringLiteral("allowPrompt");
    case PcCommandPolicy::AllowAlways:     return QStringLiteral("allowAlways");
    }
    return QStringLiteral("deny");
}

// Returns the default DenyAndAlert on any unrecognised string. This keeps the
// fall-through path safe (never run) while also surfacing the typo/unknown
// value to the user via the alert, instead of silently swallowing it.
PcCommandPolicy pcCommandPolicyFromString(const QString &s) {
    if (s == QStringLiteral("deny"))         return PcCommandPolicy::Deny;
    if (s == QStringLiteral("allowPrompt"))  return PcCommandPolicy::AllowWithPrompt;
    if (s == QStringLiteral("allowAlways"))  return PcCommandPolicy::AllowAlways;
    return PcCommandPolicy::DenyAndAlert;
}

} // namespace

QJsonObject SessionConfig::toJson() const {
    QJsonObject json;
    json["name"] = m_name;
    json["hostname"] = m_hostname;
    json["port"] = m_port;
    json["useTLS"] = m_useTLS;
    if (m_allowInvalidCertificates)
        json["allowInvalidCertificates"] = m_allowInvalidCertificates;
    json["deviceType"] = m_deviceType;
    if (!m_deviceName.isEmpty())
        json["deviceName"] = m_deviceName;
    json["screenRows"] = m_screenRows;
    json["screenCols"] = m_screenCols;
    json["codePage"] = static_cast<int>(m_codePage);
    json["terminalTheme"] = m_terminalThemeId;
    if (!m_startupScriptSource.isEmpty()) {
        json["startupScriptSource"] = m_startupScriptSource;
        json["startupScriptName"] = m_startupScriptName;
    }
    if (!m_sessionVariables.isEmpty()) {
        QJsonObject vars;
        for (auto it = m_sessionVariables.constBegin(); it != m_sessionVariables.constEnd(); ++it)
            vars[it.key()] = it.value();
        json["sessionVariables"] = vars;
    }
    if (!m_username.isEmpty())
        json["username"] = m_username;
    if (!m_password.isEmpty())
        json["password"] = m_password;
    // Only persist the policy when it diverges from the default (DenyAndAlert),
    // so existing config files do not gain a surprising new key on first save.
    if (m_pcCommandPolicy != PcCommandPolicy::DenyAndAlert)
        json["pcCommandPolicy"] = pcCommandPolicyToString(m_pcCommandPolicy);
    return json;
}

bool SessionConfig::fromJson(const QJsonObject &json) {
    // Validate numeric fields up front so the object is not mutated into a
    // half-populated state when JSON carries out-of-range values.
    const bool hasPort = json.contains("port") && json["port"].isDouble();
    const bool hasRows = json.contains("screenRows") && json["screenRows"].isDouble();
    const bool hasCols = json.contains("screenCols") && json["screenCols"].isDouble();
    const bool hasCp = json.contains("codePage") && json["codePage"].isDouble();
    int port = m_port;
    int rows = m_screenRows;
    int cols = m_screenCols;
    int cp = static_cast<int>(m_codePage);
    if (hasPort) {
        port = json["port"].toInt();
        if (port < 1 || port > 65535) return false;
    }
    if (hasRows) {
        rows = json["screenRows"].toInt();
        if (rows < 1 || rows > 132) return false;
    }
    if (hasCols) {
        cols = json["screenCols"].toInt();
        if (cols < 1 || cols > 200) return false;
    }
    if (hasCp) {
        cp = json["codePage"].toInt();
        if (!core::CodePage::isKnownId(cp)) return false;
    }

    if (json.contains("name") && json["name"].isString()) {
        m_name = json["name"].toString();
    }
    if (json.contains("hostname") && json["hostname"].isString()) {
        m_hostname = json["hostname"].toString();
    }
    if (hasPort) {
        m_port = static_cast<quint16>(port);
    }
    if (json.contains("useTLS") && json["useTLS"].isBool()) {
        m_useTLS = json["useTLS"].toBool();
    }
    if (json.contains("allowInvalidCertificates") && json["allowInvalidCertificates"].isBool()) {
        m_allowInvalidCertificates = json["allowInvalidCertificates"].toBool();
    } else {
        m_allowInvalidCertificates = false;
    }
    if (json.contains("deviceType") && json["deviceType"].isString()) {
        m_deviceType = json["deviceType"].toString();
    } else if (json.contains("deviceName") && json["deviceName"].isString()) {
        // Backward compat: old configs stored the device type in "deviceName"
        m_deviceType = json["deviceName"].toString();
    }
    if (json.contains("deviceName") && json["deviceName"].isString()
        && json.contains("deviceType")) {
        // New format: deviceName is the workstation identifier
        m_deviceName = json["deviceName"].toString();
    }
    if (hasRows) {
        m_screenRows = rows;
    }
    if (hasCols) {
        m_screenCols = cols;
    }
    if (hasCp) {
        m_codePage = static_cast<core::CodePage::ID>(cp);
    }
    if (json.contains("terminalTheme") && json["terminalTheme"].isString()) {
        m_terminalThemeId = json["terminalTheme"].toString();
    }
    if (json.contains("startupScriptSource") && json["startupScriptSource"].isString()) {
        m_startupScriptSource = json["startupScriptSource"].toString();
    }
    if (json.contains("startupScriptName") && json["startupScriptName"].isString()) {
        m_startupScriptName = json["startupScriptName"].toString();
    }
    if (json.contains("sessionVariables") && json["sessionVariables"].isObject()) {
        m_sessionVariables.clear();
        QJsonObject vars = json["sessionVariables"].toObject();
        for (auto it = vars.constBegin(); it != vars.constEnd(); ++it)
            m_sessionVariables[it.key()] = it.value().toString();
    }
    if (json.contains("username") && json["username"].isString()) {
        m_username = json["username"].toString();
    }
    if (json.contains("password") && json["password"].isString()) {
        m_password = json["password"].toString();
    }
    // STRPCCMD policy. Prefer the new "pcCommandPolicy" string field; fall
    // back to the older boolean pair for configs written by an earlier build:
    //   pcCommandEnabled=false (or absent)                   -> DenyAndAlert (default)
    //   pcCommandEnabled=true,  confirmEachTime=true|absent  -> AllowWithPrompt
    //   pcCommandEnabled=true,  confirmEachTime=false        -> AllowAlways
    // Absence of every key also lands on DenyAndAlert — never run on
    // ambiguous configs, but always make refused attempts visible.
    if (json.contains("pcCommandPolicy") && json["pcCommandPolicy"].isString()) {
        m_pcCommandPolicy = pcCommandPolicyFromString(json["pcCommandPolicy"].toString());
    } else if (json.contains("pcCommandEnabled") && json["pcCommandEnabled"].isBool()
               && json["pcCommandEnabled"].toBool()) {
        const bool confirmEachTime =
            !json.contains("pcCommandConfirmEachTime")
            || !json["pcCommandConfirmEachTime"].isBool()
            || json["pcCommandConfirmEachTime"].toBool();
        m_pcCommandPolicy = confirmEachTime
            ? PcCommandPolicy::AllowWithPrompt
            : PcCommandPolicy::AllowAlways;
    } else {
        m_pcCommandPolicy = PcCommandPolicy::DenyAndAlert;
    }
    emit changed();
    return isValid();
}

bool SessionConfig::isValid() const {
    if (m_name.isEmpty() || m_hostname.isEmpty() || m_port == 0) {
        return false;
    }
    return m_screenRows > 0 && m_screenRows <= 132 &&
           m_screenCols > 0 && m_screenCols <= 200;
}

} // namespace session
