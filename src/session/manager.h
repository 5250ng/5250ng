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

#include "session/config.h"
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

namespace session {

// Manager for session profiles (save, load, delete)
class SessionManager : public QObject {
    Q_OBJECT

  public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager();

    // Session management
    bool saveSession(const SessionConfig &config);
    bool loadSession(const QString &name, SessionConfig &config);
    bool deleteSession(const QString &name);
    QStringList listSessions() const;
    bool sessionExists(const QString &name) const;

    // Default session
    SessionConfig defaultSession() const;
    void setDefaultSession(const SessionConfig &config);

  signals:
    void sessionSaved(const QString &name);
    void sessionDeleted(const QString &name);

  private:
    QString sessionsDirectory() const;
    QString sessionFilePath(const QString &name) const;
    void ensureSessionsDirectory() const;

    QMap<QString, SessionConfig> m_sessions;
    QString m_sessionsDir;
};

} // namespace session
