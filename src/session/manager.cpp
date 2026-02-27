#include "session/manager.h"
#include "logger/logger.h"
#include "session/config.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace session {

SessionManager::SessionManager(QObject *parent) : QObject(parent) {
    // Use application data directory for sessions
    QString appDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_sessionsDir = appDataDir + "/sessions";
    ensureSessionsDirectory();

    // Load existing sessions
    QDir dir(m_sessionsDir);
    QStringList filters;
    filters << "*.json";
    QStringList files = dir.entryList(filters, QDir::Files);

    for (const QString &file : files) {
        QString sessionName = QFileInfo(file).baseName();
        SessionConfig config;
        if (loadSession(sessionName, config)) {
            m_sessions[sessionName] = config;
        }
    }
}

SessionManager::~SessionManager() {}

QString SessionManager::sessionsDirectory() const { return m_sessionsDir; }

QString SessionManager::sessionFilePath(const QString &name) const {
    // Sanitize name for filename
    QString safeName = name;
    safeName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    return m_sessionsDir + "/" + safeName + ".json";
}

void SessionManager::ensureSessionsDirectory() const {
    QDir dir;
    if (!dir.exists(m_sessionsDir)) {
        dir.mkpath(m_sessionsDir);
    }
}

bool SessionManager::saveSession(const SessionConfig &config) {
    if (!config.isValid()) {
        logger::Logger::instance()->warning("SessionManager: Cannot save invalid session");
        return false;
    }

    QString filePath = sessionFilePath(config.name());
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        logger::Logger::instance()->warning(QString("SessionManager: Cannot open file for writing: %1").arg(filePath));
        return false;
    }

    QJsonObject json = config.toJson();
    QJsonDocument doc(json);
    file.write(doc.toJson());
    file.close();

    m_sessions[config.name()] = config;
    emit sessionSaved(config.name());

    return true;
}

bool SessionManager::loadSession(const QString &name, SessionConfig &config) {
    QString filePath = sessionFilePath(name);
    QFile file(filePath);

    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        logger::Logger::instance()->warning(QString("SessionManager: Cannot open file for reading: %1").arg(filePath));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        logger::Logger::instance()->warning(QString("SessionManager: Invalid JSON in session file: %1").arg(filePath));
        return false;
    }

    return config.fromJson(doc.object());
}

bool SessionManager::deleteSession(const QString &name) {
    QString filePath = sessionFilePath(name);
    QFile file(filePath);

    if (file.exists()) {
        if (!file.remove()) {
            logger::Logger::instance()->warning(QString("SessionManager: Cannot delete session file: %1").arg(filePath));
            return false;
        }
    }

    m_sessions.remove(name);
    emit sessionDeleted(name);

    return true;
}

QStringList SessionManager::listSessions() const { return m_sessions.keys(); }

bool SessionManager::sessionExists(const QString &name) const {
    return m_sessions.contains(name);
}

SessionConfig SessionManager::defaultSession() const {
    SessionConfig config;
    config.setName("Default");
    config.setHostname("localhost");
    config.setPort(23);
    config.setUseTLS(false);
    config.setDeviceName("IBM-3179-2");
    config.setScreenRows(24);
    config.setScreenCols(80);
    return config;
}

void SessionManager::setDefaultSession(const SessionConfig &config) {
    saveSession(config);
}

} // namespace session
