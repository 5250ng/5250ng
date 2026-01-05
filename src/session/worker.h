#pragma once

#include "network/tn5250/client/client.h"
#include "session/config.h"
#include "logger/logger.h"
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QStringList>

namespace tn5250::session {

class Worker : public QObject {
    Q_OBJECT
  public:
    explicit Worker(QObject *parent = nullptr);
    ~Worker();

    void setConfig(const ::session::SessionConfig &cfg);

  public slots:
    void start();
    void stop();
    void sendInput(const QByteArray &data);

  signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void stateChanged(tn5250::client::TN5250Client::ConnectionState state);
    void appData(const QByteArray &data);
    void sessionLogAppended(const QString &line);

  private:
    ::session::SessionConfig m_config;
    tn5250::client::TN5250Client *m_client;
    QStringList m_logs;
    QMutex m_logsMutex;

    void onClientData(const QByteArray &data);
    void onGlobalLogMessage(logger::LogLevel level, const QString &message);

  public:
    // Access a snapshot of current logs
    QStringList logs() const {
        QMutexLocker locker(&const_cast<Worker *>(this)->m_logsMutex);
        return m_logs;
    }
};

} // namespace tn5250::session
