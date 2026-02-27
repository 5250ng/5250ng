#include "session/config.h"
#include <QJsonDocument>
#include <QJsonObject>

namespace session {

SessionConfig::SessionConfig(QObject *parent) : QObject(parent), m_name("New Session"), m_hostname(""), m_port(23), m_useTLS(false), m_deviceName("QT5250"), m_screenRows(24), m_screenCols(80) {}

SessionConfig::SessionConfig(const SessionConfig &other) : QObject(other.parent()), m_name(other.m_name), m_hostname(other.m_hostname), m_port(other.m_port), m_useTLS(other.m_useTLS), m_deviceName(other.m_deviceName), m_screenRows(other.m_screenRows), m_screenCols(other.m_screenCols) {}

SessionConfig &SessionConfig::operator=(const SessionConfig &other) {
    if (this != &other) {
        m_name = other.m_name;
        m_hostname = other.m_hostname;
        m_port = other.m_port;
        m_useTLS = other.m_useTLS;
        m_deviceName = other.m_deviceName;
        m_screenRows = other.m_screenRows;
        m_screenCols = other.m_screenCols;
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

    emit changed();
    return isValid();
}

bool SessionConfig::isValid() const {
    return !m_name.isEmpty() && !m_hostname.isEmpty() && m_port > 0 &&
           m_port <= 65535 && m_screenRows > 0 && m_screenRows <= 132 &&
           m_screenCols > 0 && m_screenCols <= 200;
}

} // namespace session
