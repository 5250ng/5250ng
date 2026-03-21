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

#include "network/tn5250_qt/client/client.h"
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
    void sendAttention();
    void sendSystemRequest();
    void sendGDS(uint8_t flagsHi, uint8_t opcode, const QByteArray &payload);

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
    mutable QMutex m_logsMutex;

    void onClientData(const QByteArray &data);
    void onGlobalLogMessage(logger::LogLevel level, const QString &message);

  public:
    // Access a snapshot of current logs
    QStringList logs() const {
        QMutexLocker locker(&m_logsMutex);
        return m_logs;
    }
};

} // namespace tn5250::session
