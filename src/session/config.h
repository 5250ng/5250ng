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

#include "core/codepage.h"
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <cstdint>

namespace session {

// Session configuration for TN5250 connections
class SessionConfig : public QObject {
    Q_OBJECT

  public:
    explicit SessionConfig(QObject *parent = nullptr);
    SessionConfig(const SessionConfig &other);
    SessionConfig &operator=(const SessionConfig &other);

    // Session identification
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    // Connection settings
    QString hostname() const { return m_hostname; }
    void setHostname(const QString &hostname) { m_hostname = hostname; }

    quint16 port() const { return m_port; }
    void setPort(quint16 port) { m_port = port; }

    bool useTLS() const { return m_useTLS; }
    void setUseTLS(bool useTLS) { m_useTLS = useTLS; }

    // Device type is the terminal model (e.g. IBM-3179-2) sent via TERMINAL_TYPE
    QString deviceType() const { return m_deviceType; }
    void setDeviceType(const QString &type) { m_deviceType = type; }

    // Device name is the workstation identifier (e.g. MYTERM01) sent via DEVNAME.
    // Empty = server auto-assigns (recommended).
    QString deviceName() const { return m_deviceName; }
    void setDeviceName(const QString &name) { m_deviceName = name; }

    // Display settings
    int screenRows() const { return m_screenRows; }
    void setScreenRows(int rows) { m_screenRows = rows; }

    int screenCols() const { return m_screenCols; }
    void setScreenCols(int cols) { m_screenCols = cols; }

    // Code page
    core::CodePage::ID codePage() const { return m_codePage; }
    void setCodePage(core::CodePage::ID cp) { m_codePage = cp; }

    // Terminal theme
    QString terminalThemeId() const { return m_terminalThemeId; }
    void setTerminalThemeId(const QString &id) { m_terminalThemeId = id; }

    // Startup script (embedded source)
    QString startupScriptSource() const { return m_startupScriptSource; }
    void setStartupScriptSource(const QString &source) { m_startupScriptSource = source; }

    QString startupScriptName() const { return m_startupScriptName; }
    void setStartupScriptName(const QString &name) { m_startupScriptName = name; }

    // Session variables (for $SESSION_* script variables)
    QHash<QString, QString> sessionVariables() const { return m_sessionVariables; }
    void setSessionVariables(const QHash<QString, QString> &vars) { m_sessionVariables = vars; }

    // Credentials
    QString username() const { return m_username; }
    void setUsername(const QString &username) { m_username = username; }

    QString password() const { return m_password; }
    void setPassword(const QString &password) { m_password = password; }

    // Serialization
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);

    // Validation
    bool isValid() const;

  signals:
    void changed();

  private:
    QString m_name;
    QString m_hostname;
    quint16 m_port;
    bool m_useTLS;
    QString m_deviceType;
    QString m_deviceName;
    int m_screenRows;
    int m_screenCols;
    core::CodePage::ID m_codePage;
    QString m_terminalThemeId;
    QString m_startupScriptSource;
    QString m_startupScriptName;
    QHash<QString, QString> m_sessionVariables;
    QString m_username;
    QString m_password;

};

} // namespace session
