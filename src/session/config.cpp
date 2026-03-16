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
#include <QJsonDocument>
#include <QJsonObject>

namespace session {

SessionConfig::SessionConfig(QObject *parent) : QObject(parent), m_name("New Session"), m_hostname(""), m_port(23), m_useTLS(false), m_deviceName("IBM-3179-2"), m_screenRows(24), m_screenCols(80), m_codePage(core::CodePage::ID::CP037), m_terminalThemeId("classic_green") {}

SessionConfig::SessionConfig(const SessionConfig &other) : QObject(other.parent()), m_name(other.m_name), m_hostname(other.m_hostname), m_port(other.m_port), m_useTLS(other.m_useTLS), m_deviceName(other.m_deviceName), m_screenRows(other.m_screenRows), m_screenCols(other.m_screenCols), m_codePage(other.m_codePage), m_terminalThemeId(other.m_terminalThemeId), m_startupScript(other.m_startupScript), m_username(other.m_username), m_password(other.m_password) {}

SessionConfig &SessionConfig::operator=(const SessionConfig &other) {
    if (this != &other) {
        m_name = other.m_name;
        m_hostname = other.m_hostname;
        m_port = other.m_port;
        m_useTLS = other.m_useTLS;
        m_deviceName = other.m_deviceName;
        m_screenRows = other.m_screenRows;
        m_screenCols = other.m_screenCols;
        m_codePage = other.m_codePage;
        m_terminalThemeId = other.m_terminalThemeId;
        m_startupScript = other.m_startupScript;
        m_username = other.m_username;
        m_password = other.m_password;
        emit changed();
    }
    return *this;
}

QJsonObject SessionConfig::toJson() const {
    QJsonObject json;
    json["name"] = m_name;
    json["hostname"] = m_hostname;
    json["port"] = m_port;
    json["useTLS"] = m_useTLS;
    json["deviceName"] = m_deviceName;
    json["screenRows"] = m_screenRows;
    json["screenCols"] = m_screenCols;
    json["codePage"] = static_cast<int>(m_codePage);
    json["terminalTheme"] = m_terminalThemeId;
    if (!m_startupScript.isEmpty())
        json["startupScript"] = m_startupScript;
    return json;
}

bool SessionConfig::fromJson(const QJsonObject &json) {
    if (json.contains("name") && json["name"].isString()) {
        m_name = json["name"].toString();
    }
    if (json.contains("hostname") && json["hostname"].isString()) {
        m_hostname = json["hostname"].toString();
    }
    if (json.contains("port") && json["port"].isDouble()) {
        m_port = static_cast<quint16>(json["port"].toInt());
    }
    if (json.contains("useTLS") && json["useTLS"].isBool()) {
        m_useTLS = json["useTLS"].toBool();
    }
    if (json.contains("deviceName") && json["deviceName"].isString()) {
        m_deviceName = json["deviceName"].toString();
    }
    if (json.contains("screenRows") && json["screenRows"].isDouble()) {
        m_screenRows = json["screenRows"].toInt();
    }
    if (json.contains("screenCols") && json["screenCols"].isDouble()) {
        m_screenCols = json["screenCols"].toInt();
    }
    if (json.contains("codePage") && json["codePage"].isDouble()) {
        m_codePage = static_cast<core::CodePage::ID>(json["codePage"].toInt());
    }
    if (json.contains("terminalTheme") && json["terminalTheme"].isString()) {
        m_terminalThemeId = json["terminalTheme"].toString();
    }
    if (json.contains("startupScript") && json["startupScript"].isString()) {
        m_startupScript = json["startupScript"].toString();
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
