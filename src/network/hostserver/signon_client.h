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

#include "host_constants.h"
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtNetwork/QTcpSocket>
#include <cstdint>

namespace hostserver {

// Authenticates with the IBM i signon server (port 8476).
//
// The signon flow:
//   1. Connect to signon server
//   2. Exchange attributes (client version, seed)
//   3. Receive server seed + password level
//   4. Send signon info (encrypted password)
//   5. Receive signon result
//
// After signon, the credentials and password level can be reused
// to authenticate with other host servers (IFS, database, etc.)
// via the exchange seeds + start server flow.
class SignonClient : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Disconnected,
        Connecting,
        ExchangingAttributes,
        Authenticating,
        Authenticated,
        Error
    };

    explicit SignonClient(QObject *parent = nullptr);
    ~SignonClient();

    // Initiate signon to the given host
    void signon(const QString &hostname, uint16_t port,
                const QString &userId, const QString &password);

    // Convenience: signon using default port
    void signon(const QString &hostname,
                const QString &userId, const QString &password);

    State state() const { return m_state; }
    bool isAuthenticated() const { return m_state == State::Authenticated; }

    // After successful signon, these are available for authenticating
    // with other host servers
    uint8_t passwordLevel() const { return m_passwordLevel; }
    uint16_t serverLevel() const { return m_serverLevel; }
    QByteArray serverSeed() const { return m_serverSeed; }
    QByteArray clientSeed() const { return m_clientSeed; }
    QString userId() const { return m_userId; }
    QString password() const { return m_password; }
    uint32_t serverCCSID() const { return m_serverCCSID; }
    QString jobName() const { return m_jobName; }

  signals:
    void authenticated();
    void errorOccurred(const QString &error);
    void stateChanged(State state);

  private slots:
    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketDisconnected();

  private:
    void setState(State newState);
    void sendExchangeAttributes();
    void handleExchangeAttributesReply(const QByteArray &packet);
    void sendSignonInfo();
    void handleSignonReply(const QByteArray &packet);

    QTcpSocket *m_socket;
    QByteArray m_recvBuffer;
    State m_state;

    QString m_hostname;
    uint16_t m_port;
    QString m_userId;
    QString m_password;

    QByteArray m_clientSeed;
    QByteArray m_serverSeed;
    uint8_t m_passwordLevel;
    uint16_t m_serverLevel;
    uint32_t m_serverVersion;
    uint32_t m_serverCCSID;
    QString m_jobName;
};

} // namespace hostserver
