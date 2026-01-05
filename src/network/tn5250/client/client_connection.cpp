#include "client.h"
#include "logger/logger.h"
#include <QHostAddress>
#include <QString>

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
    }
}

void TN5250Client::disconnectFromHost() {
    if (m_socket) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(3000);
        }
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

void TN5250Client::setState(ConnectionState newState) {
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

} // namespace tn5250::client
