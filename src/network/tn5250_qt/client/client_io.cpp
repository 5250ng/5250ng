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

#ifdef HAVE_QT6_SSL
#include <QtNetwork/QSslError>
#include <QtNetwork/QSslSocket>
#endif

#include <QTimer>

namespace tn5250::client {

void TN5250Client::onSocketConnected() {
    logger::Logger::instance()->debug("[TN5250->Client]: Socket connected");

#ifdef HAVE_QT6_SSL
    if (m_useTLS && m_sslSocket) {
        if (static_cast<QSslSocket *>(m_sslSocket)->isEncrypted()) {
            logger::Logger::instance()->debug(
                "[TN5250->Client]: TLS connection established"
            );
            m_tlsStarted = true;
            if (m_connectTimeoutTimer) m_connectTimeoutTimer->stop();
            setState(ConnectionState::Negotiating);
            performHandshake();
        } else {
            // TLS handshake in progress
            return;
        }
    } else {
#endif
        if (m_connectTimeoutTimer) m_connectTimeoutTimer->stop();
        setState(ConnectionState::Negotiating);
        performHandshake();
#ifdef HAVE_QT6_SSL
    }
#endif
}

void TN5250Client::onSocketDisconnected() {
    logger::Logger::instance()->debug("[TN5250->Client]: Socket disconnected");
    stopHeartbeat();
    setState(ConnectionState::Disconnected);
    emit disconnected();
}

void TN5250Client::onSocketReadyRead() {
    if (!m_socket) {
        return;
    }

    QByteArray data = m_socket->readAll();
    logger::Logger::instance()->debug(
        QString("[TN5250->Client]: Socket readyRead - received %1 raw bytes")
            .arg(data.size())
    );
    processTelnetData(data);
}

void TN5250Client::onSocketError(QAbstractSocket::SocketError /*error*/) {
    QString errorString = m_socket ? m_socket->errorString() : "Unknown error";
    logger::Logger::instance()->error(
        QString("[TN5250->Client]: Socket error: %1").arg(errorString)
    );
    setState(ConnectionState::Error);
    emit errorOccurred(errorString);
}

#ifdef HAVE_QT6_SSL
void TN5250Client::onSslErrors(const QList<QSslError> &errors) {
    QStringList errorStrings;
    for (const QSslError &err : errors) {
        errorStrings << err.errorString();
        logger::Logger::instance()->warning(
            QString("[TN5250->Client]: SSL error: %1").arg(err.errorString()));
    }

    if (!m_allowInvalidCertificates) {
        const QString combined = errorStrings.join("; ");
        logger::Logger::instance()->error(
            QString("[TN5250->Client]: TLS certificate validation failed: %1").arg(combined));
        setState(ConnectionState::Error);
        emit errorOccurred(QString("TLS certificate validation failed: %1").arg(combined));
        if (m_sslSocket) {
            static_cast<QSslSocket *>(m_sslSocket)->abort();
        }
        return;
    }

    logger::Logger::instance()->warning(
        "[TN5250->Client]: Ignoring SSL errors (allowInvalidCertificates=true)");
    if (m_sslSocket) {
        static_cast<QSslSocket *>(m_sslSocket)->ignoreSslErrors();
    }
}
#endif

} // namespace tn5250::client
