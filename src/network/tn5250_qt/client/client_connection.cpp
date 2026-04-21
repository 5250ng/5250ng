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

#include "client.h"
#include "logger/logger.h"
#include <QHostAddress>
#include <QString>
#include <QTimer>

#ifdef HAVE_QT6_SSL
#include <QtNetwork/QSslSocket>
#endif

namespace tn5250::client {

using telnet::TelnetOption;

TN5250Client::TN5250Client(QObject *parent) : QObject(parent), m_socket(nullptr), m_sslSocket(nullptr), m_tcpSocket(nullptr), m_useTLS(false), m_tlsStarted(false), m_state(ConnectionState::Disconnected), m_port(23), m_deviceName("IBM-3179-2"), m_inSubnegotiation(false), m_currentSubnegotiation(TelnetOption::TRANSMIT_BINARY), m_handshakeComplete(false), m_binaryNegotiated(false), m_eorNegotiated(false), m_terminalTypeSent(false) {}

TN5250Client::~TN5250Client() { disconnectFromHost(); }

void TN5250Client::connectToHost(const QString &hostname, quint16 port, bool useTLS) {
    if (m_state != ConnectionState::Disconnected) {
        logger::Logger::instance()->warning(
            "[TN5250->Client]: Already connected or connecting"
        );
        return;
    }

    m_hostname = hostname;
    m_port = port;
    m_useTLS = useTLS;
    m_tlsStarted = false;
    m_handshakeComplete = false;
    m_binaryNegotiated = false;
    m_eorNegotiated = false;
    m_terminalTypeSent = false;
    m_deviceNameSent = false;
    m_receiveBuffer.clear();
    m_handshakeBuffer.clear();
    m_inSubnegotiation = false;

    setState(ConnectionState::Connecting);

#ifdef HAVE_QT6_SSL
    if (useTLS) {
        m_sslSocket = new QSslSocket(this);
        m_socket = m_sslSocket;

        connect(m_sslSocket, &QSslSocket::connected, this, &TN5250Client::onSocketConnected);
        connect(m_sslSocket, &QSslSocket::disconnected, this, &TN5250Client::onSocketDisconnected);
        connect(m_sslSocket, &QSslSocket::readyRead, this, &TN5250Client::onSocketReadyRead);
        connect(
            m_sslSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
            this, &TN5250Client::onSocketError
        );
#ifdef HAVE_QT6_SSL
        connect(m_sslSocket, &QSslSocket::sslErrors, this, &TN5250Client::onSslErrors);
#endif

        // Start TLS after connection
        m_sslSocket->connectToHostEncrypted(hostname, port);

        armConnectTimeout();
    } else {
#else
    if (useTLS) {
        QString errorMsg =
            "TLS/SSL support is not available. "
            "Please install Qt6 SSL libraries (libqt6network6, libssl-dev) "
            "and rebuild the application.";
        logger::Logger::instance()->error(
            "[TN5250->Client]: TLS requested but SSL support not available"
        );
        emit errorOccurred(errorMsg);
        return;
    } else {
#endif
        m_tcpSocket = new QTcpSocket(this);
        m_socket = m_tcpSocket;

        connect(m_tcpSocket, &QTcpSocket::connected, this, &TN5250Client::onSocketConnected);
        connect(m_tcpSocket, &QTcpSocket::disconnected, this, &TN5250Client::onSocketDisconnected);
        connect(m_tcpSocket, &QTcpSocket::readyRead, this, &TN5250Client::onSocketReadyRead);
        connect(
            m_tcpSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &TN5250Client::onSocketError
        );

        m_tcpSocket->connectToHost(hostname, port);

        armConnectTimeout();
    }
}

void TN5250Client::armConnectTimeout() {
    if (!m_connectTimeoutTimer) {
        m_connectTimeoutTimer = new QTimer(this);
        m_connectTimeoutTimer->setSingleShot(true);
        connect(m_connectTimeoutTimer, &QTimer::timeout,
                this, &TN5250Client::onConnectTimeout);
    }
    m_connectTimeoutTimer->start(30000);
}

void TN5250Client::onConnectTimeout() {
    if (m_state != ConnectionState::Connecting) return;
    const char *what = m_useTLS ? "TLS handshake" : "TCP connection";
    logger::Logger::instance()->error(
        QString("[TN5250->Client]: %1 timed out after 30 seconds").arg(what));
    disconnectFromHost();
    emit errorOccurred(m_useTLS ? QStringLiteral("TLS handshake timed out")
                                : QStringLiteral("Connection timed out"));
}

void TN5250Client::disconnectFromHost() {
    stopHeartbeat();

    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }

    if (m_socket) {
        // Abort instead of blocking - avoids freezing the thread for up to 3s
        m_socket->abort();
    }

#ifdef HAVE_QT6_SSL
    if (m_sslSocket) {
        static_cast<QSslSocket *>(m_sslSocket)->deleteLater();
        m_sslSocket = nullptr;
    }
#endif

    if (m_tcpSocket) {
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }

    m_socket = nullptr;
    setState(ConnectionState::Disconnected);
}

bool TN5250Client::isConnected() const {
    return m_state == ConnectionState::Connected && m_socket &&
           m_socket->state() == QAbstractSocket::ConnectedState;
}

void TN5250Client::setDeviceName(const QString &deviceName) {
    m_deviceName = deviceName;
}

void TN5250Client::setCredentials(const QString &username, const QString &password) {
    m_username = username;
    m_password = password;
}

void TN5250Client::setState(ConnectionState newState) {
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

} // namespace tn5250::client
