#pragma once

#include "session_config.h"
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

namespace core {

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

} // namespace core
