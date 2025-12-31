#include "tn5250_client.h"
#include "../core/logger.h"
#include "telnet_options.h"
#include <QDebug>
#include <QHostAddress>

#ifdef HAVE_QT6_SSL
#include <QSslError>
#include <QSslSocket>
#endif

namespace transport {

TN5250Client::TN5250Client(QObject *parent)
    : QObject(parent), m_socket(nullptr), m_sslSocket(nullptr),
      m_tcpSocket(nullptr), m_useTLS(false), m_tlsStarted(false),
      m_state(ConnectionState::Disconnected), m_port(23),
      m_deviceName("QT5250"), m_inSubnegotiation(false),
      m_currentSubnegotiation(TelnetOption::BINARY),
      m_handshakeComplete(false) {}

TN5250Client::~TN5250Client() { disconnectFromHost(); }

void TN5250Client::connectToHost(const QString &hostname, quint16 port,
                                 bool useTLS) {
  if (m_state != ConnectionState::Disconnected) {
    core::Logger::instance()->warning(
        "TN5250Client: Already connected or connecting");
    return;
  }

  m_hostname = hostname;
  m_port = port;
  m_useTLS = useTLS;
  m_tlsStarted = false;
  m_handshakeComplete = false;
  m_receiveBuffer.clear();
  m_handshakeBuffer.clear();
  m_inSubnegotiation = false;

  setState(ConnectionState::Connecting);

#ifdef HAVE_QT6_SSL
  if (useTLS) {
    m_sslSocket = new QSslSocket(this);
    m_socket = m_sslSocket;

    connect(m_sslSocket, &QSslSocket::connected, this,
            &TN5250Client::onSocketConnected);
    connect(m_sslSocket, &QSslSocket::disconnected, this,
            &TN5250Client::onSocketDisconnected);
    connect(m_sslSocket, &QSslSocket::readyRead, this,
            &TN5250Client::onSocketReadyRead);
    connect(
        m_sslSocket,
        QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
        this, &TN5250Client::onSocketError);
#ifdef HAVE_QT6_SSL
    connect(m_sslSocket, &QSslSocket::sslErrors, this,
            &TN5250Client::onSslErrors);
#endif

    // Start TLS after connection
    m_sslSocket->connectToHostEncrypted(hostname, port);
  } else {
#else
  if (useTLS) {
    QString errorMsg = "TLS/SSL support is not available. "
                      "Please install Qt6 SSL libraries (libqt6network6, libssl-dev) "
                      "and rebuild the application.";
    core::Logger::instance()->error(
        "TN5250Client: TLS requested but SSL support not available");
    emit errorOccurred(errorMsg);
    return;
  } else {
#endif
    m_tcpSocket = new QTcpSocket(this);
    m_socket = m_tcpSocket;

    connect(m_tcpSocket, &QTcpSocket::connected, this,
            &TN5250Client::onSocketConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this,
            &TN5250Client::onSocketDisconnected);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this,
            &TN5250Client::onSocketReadyRead);
    connect(
        m_tcpSocket,
        QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
        this, &TN5250Client::onSocketError);

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

void TN5250Client::sendData(const QByteArray &data) {
  if (!isConnected()) {
    core::Logger::instance()->warning(
        "TN5250Client: Cannot send data, not connected");
    return;
  }

  sendRawData(data);
}

void TN5250Client::sendRawData(const QByteArray &data) {
  if (!m_socket) {
    return;
  }

  // Escape IAC bytes in data
  QByteArray escaped;
  for (uint8_t byte : data) {
    if (byte == static_cast<uint8_t>(TelnetCommand::IAC)) {
      escaped.append(static_cast<uint8_t>(TelnetCommand::IAC));
      escaped.append(static_cast<uint8_t>(TelnetCommand::IAC));
    } else {
      escaped.append(byte);
    }
  }

  m_socket->write(escaped);
}

void TN5250Client::setDeviceName(const QString &deviceName) {
  m_deviceName = deviceName;
}

void TN5250Client::onSocketConnected() {
  core::Logger::instance()->info("TN5250Client: Socket connected");

#ifdef HAVE_QT6_SSL
  if (m_useTLS && m_sslSocket) {
    if (static_cast<QSslSocket *>(m_sslSocket)->isEncrypted()) {
      core::Logger::instance()->info(
          "TN5250Client: TLS connection established");
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
  core::Logger::instance()->info("TN5250Client: Socket disconnected");
  setState(ConnectionState::Disconnected);
  emit disconnected();
}

void TN5250Client::onSocketReadyRead() {
  if (!m_socket) {
    return;
  }

  QByteArray data = m_socket->readAll();
  processTelnetData(data);
}

void TN5250Client::onSocketError(QAbstractSocket::SocketError error) {
  QString errorString = m_socket ? m_socket->errorString() : "Unknown error";
  core::Logger::instance()->error(
      QString("TN5250Client: Socket error: %1").arg(errorString));
  setState(ConnectionState::Error);
  emit errorOccurred(errorString);
}

#ifdef HAVE_QT6_SSL
void TN5250Client::onSslErrors(const QList<QSslError> &errors) {
  core::Logger::instance()->warning("TN5250Client: SSL errors occurred");
  // For now, we'll accept the connection anyway
  // In production, this should be configurable
  if (m_sslSocket) {
    static_cast<QSslSocket *>(m_sslSocket)->ignoreSslErrors();
  }
}
#endif

void TN5250Client::setState(ConnectionState newState) {
  if (m_state != newState) {
    m_state = newState;
    emit stateChanged(newState);
  }
}

void TN5250Client::processTelnetData(const QByteArray &data) {
  m_receiveBuffer.append(data);

  QByteArray processed;
  int i = 0;

  while (i < m_receiveBuffer.size()) {
    uint8_t byte = static_cast<uint8_t>(m_receiveBuffer[i]);

    if (byte == static_cast<uint8_t>(TelnetCommand::IAC)) {
      if (i + 1 >= m_receiveBuffer.size()) {
        // Need more data
        break;
      }

      uint8_t next = static_cast<uint8_t>(m_receiveBuffer[i + 1]);

      // Double IAC means literal IAC
      if (next == static_cast<uint8_t>(TelnetCommand::IAC)) {
        processed.append(static_cast<uint8_t>(TelnetCommand::IAC));
        i += 2;
        continue;
      }

      // Check if it's a standalone command (no option byte)
      if (next == static_cast<uint8_t>(TelnetCommand::SE) ||
          next == static_cast<uint8_t>(TelnetCommand::NOP) ||
          next == static_cast<uint8_t>(TelnetCommand::DM) ||
          next == static_cast<uint8_t>(TelnetCommand::BRK) ||
          next == static_cast<uint8_t>(TelnetCommand::IP) ||
          next == static_cast<uint8_t>(TelnetCommand::AO) ||
          next == static_cast<uint8_t>(TelnetCommand::AYT) ||
          next == static_cast<uint8_t>(TelnetCommand::EC) ||
          next == static_cast<uint8_t>(TelnetCommand::EL) ||
          next == static_cast<uint8_t>(TelnetCommand::GA)) {

        // Standalone command - handle it
        if (next == static_cast<uint8_t>(TelnetCommand::SE)) {
          // SE can end a subnegotiation
          if (m_inSubnegotiation) {
            handleSubnegotiation(m_currentSubnegotiation,
                                 m_subnegotiationBuffer);
            m_subnegotiationBuffer.clear();
            m_inSubnegotiation = false;
          }
        } else {
          // Other standalone commands - just skip for now
          // Could add specific handling if needed
        }
        i += 2;
        continue;
      }

      // Command with option byte (WILL, WONT, DO, DONT, SB)
      if (i + 2 >= m_receiveBuffer.size()) {
        // Need more data
        break;
      }

      uint8_t cmd = next;
      uint8_t opt = static_cast<uint8_t>(m_receiveBuffer[i + 2]);

      handleTelnetCommand(cmd, opt);
      i += 3;
    } else if (m_inSubnegotiation) {
      m_subnegotiationBuffer.append(byte);
      i++;
    } else {
      processed.append(byte);
      i++;
    }
  }

  // Remove processed data from buffer
  if (i > 0) {
    m_receiveBuffer.remove(0, i);
  }

  // Process application data
  if (!processed.isEmpty()) {
    if (m_handshakeComplete) {
      emit dataReceived(processed);
    } else {
      m_handshakeBuffer.append(processed);
      processHandshakeData(processed);
    }
  }
}

void TN5250Client::sendTelnetCommand(TelnetCommand cmd, TelnetOption opt) {
  if (!m_socket) {
    return;
  }

  QByteArray command;
  command.append(static_cast<uint8_t>(TelnetCommand::IAC));
  command.append(static_cast<uint8_t>(cmd));
  command.append(static_cast<uint8_t>(opt));

  m_socket->write(command);
}

void TN5250Client::handleTelnetCommand(uint8_t cmd, uint8_t opt) {
  TelnetCommand command = static_cast<TelnetCommand>(cmd);
  TelnetOption option = static_cast<TelnetOption>(opt);

  switch (command) {
  case TelnetCommand::WILL:
    // Server wants to enable an option
    if (option == TelnetOption::BINARY) {
      sendTelnetCommand(TelnetCommand::DO, TelnetOption::BINARY);
    } else if (option == TelnetOption::EOR) {
      sendTelnetCommand(TelnetCommand::DO, TelnetOption::EOR);
    } else if (option == TelnetOption::START_TLS) {
      handleStartTLS();
    } else {
      sendTelnetCommand(TelnetCommand::DONT, option);
    }
    break;

  case TelnetCommand::WONT:
    // Server refuses an option
    break;

  case TelnetCommand::DO:
    // Server wants us to enable an option
    if (option == TelnetOption::BINARY) {
      sendTelnetCommand(TelnetCommand::WILL, TelnetOption::BINARY);
    } else if (option == TelnetOption::EOR) {
      sendTelnetCommand(TelnetCommand::WILL, TelnetOption::EOR);
    } else {
      sendTelnetCommand(TelnetCommand::WONT, option);
    }
    break;

  case TelnetCommand::DONT:
    // Server wants us to disable an option
    break;

  case TelnetCommand::SB:
    // Start subnegotiation
    m_inSubnegotiation = true;
    m_currentSubnegotiation = option;
    m_subnegotiationBuffer.clear();
    break;

  default:
    core::Logger::instance()->debug(
        QString("TN5250Client: Unhandled telnet command: %1").arg(cmd));
    break;
  }
}

void TN5250Client::handleSubnegotiation(TelnetOption opt,
                                        const QByteArray &data) {
  switch (opt) {
  case TelnetOption::TERMINAL_TYPE:
    // Handle terminal type subnegotiation
    core::Logger::instance()->debug(
        "TN5250Client: Terminal type subnegotiation");
    break;

  case TelnetOption::NAWS:
    // Handle window size subnegotiation
    core::Logger::instance()->debug("TN5250Client: NAWS subnegotiation");
    break;

  default:
    core::Logger::instance()->debug(
        QString("TN5250Client: Unhandled subnegotiation for option: %1")
            .arg(static_cast<int>(opt)));
    break;
  }
}

void TN5250Client::performHandshake() {
  core::Logger::instance()->info("TN5250Client: Starting TN5250 handshake");

  // Negotiate binary mode
  sendTelnetCommand(TelnetCommand::WILL, TelnetOption::BINARY);
  sendTelnetCommand(TelnetCommand::DO, TelnetOption::BINARY);

  // Negotiate EOR (End of Record)
  sendTelnetCommand(TelnetCommand::WILL, TelnetOption::EOR);
  sendTelnetCommand(TelnetCommand::DO, TelnetOption::EOR);

  // Send device name
  sendDeviceName();
}

void TN5250Client::sendDeviceName() {
  // TN5250 device name negotiation
  // Format: IAC SB TERMINAL_TYPE SEND <device_name> IAC SE
  QByteArray negotiation;
  negotiation.append(static_cast<uint8_t>(TelnetCommand::IAC));
  negotiation.append(static_cast<uint8_t>(TelnetCommand::SB));
  negotiation.append(static_cast<uint8_t>(TelnetOption::TERMINAL_TYPE));
  negotiation.append(static_cast<uint8_t>(0)); // SEND
  negotiation.append(m_deviceName.toUtf8());
  negotiation.append(static_cast<uint8_t>(TelnetCommand::IAC));
  negotiation.append(static_cast<uint8_t>(TelnetCommand::SE));

  if (m_socket) {
    m_socket->write(negotiation);
  }
}

void TN5250Client::processHandshakeData(const QByteArray &data) {
  // For now, consider handshake complete after receiving any data
  // In a full implementation, we'd wait for specific handshake responses
  if (!m_handshakeBuffer.isEmpty() && !m_handshakeComplete) {
    m_handshakeComplete = true;
    setState(ConnectionState::Connected);
    emit connected();

    // Process any buffered data
    if (m_handshakeBuffer.size() > 0) {
      emit dataReceived(m_handshakeBuffer);
      m_handshakeBuffer.clear();
    }
  }
}

void TN5250Client::startTLS() {
#ifdef HAVE_QT6_SSL
  if (!m_sslSocket || m_tlsStarted) {
    return;
  }

  sendTelnetCommand(TelnetCommand::DO, TelnetOption::START_TLS);
#else
  core::Logger::instance()->warning("TN5250Client: TLS not supported");
#endif
}

void TN5250Client::handleStartTLS() {
#ifdef HAVE_QT6_SSL
  if (m_sslSocket && !m_tlsStarted) {
    core::Logger::instance()->info("TN5250Client: Starting TLS negotiation");
    static_cast<QSslSocket *>(m_sslSocket)->startClientEncryption();
  }
#else
  core::Logger::instance()->warning("TN5250Client: TLS not supported");
#endif
}

} // namespace transport
