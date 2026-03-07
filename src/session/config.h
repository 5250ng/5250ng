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

    QString deviceName() const { return m_deviceName; }
    void setDeviceName(const QString &deviceName) { m_deviceName = deviceName; }

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

    // Credentials (transient — not serialized to JSON)
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
    QString m_deviceName;
    int m_screenRows;
    int m_screenCols;
    core::CodePage::ID m_codePage;
    QString m_terminalThemeId;
    QString m_username;
    QString m_password;

};

} // namespace session
