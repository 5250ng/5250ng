#include "client.h"
#include "logger/logger.h"

#ifdef HAVE_QT6_SSL
#include <QtNetwork/QSslError>
#include <QtNetwork/QSslSocket>
#endif

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
            setState(ConnectionState::Negotiating);
            performHandshake();
        } else {
            // TLS handshake in progress
            return;
        }
    } else {
#endif
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
void TN5250Client::onSslErrors(const QList<QSslError> & /*errors*/) {
    logger::Logger::instance()->warning("[TN5250->Client]: SSL errors occurred");
    // For now, we'll accept the connection anyway
    // In production, this should be configurable
    if (m_sslSocket) {
        static_cast<QSslSocket *>(m_sslSocket)->ignoreSslErrors();
    }
}
#endif

} // namespace tn5250::client
