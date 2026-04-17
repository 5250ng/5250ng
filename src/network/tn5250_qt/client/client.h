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

#include "core/codepage.h"
#include "network/tn5250_qt/telnet/commands.h"
#include "network/tn5250_qt/telnet/options.h"
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QTcpSocket>
#include <memory>

// Forward declarations for optional SSL support
class QSslSocket;
class QSslError;

namespace tn5250::client {

class TN5250Client : public QObject {
    Q_OBJECT

  public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Negotiating,
        Connected,
        Error
    };

    explicit TN5250Client(QObject *parent = nullptr);
    ~TN5250Client();

    // Connection management
    void connectToHost(const QString &hostname, quint16 port, bool useTLS = false);
    void disconnectFromHost();
    bool isConnected() const;
    ConnectionState state() const { return m_state; }

    // Data transmission
    void sendData(const QByteArray &data);
    void sendRawData(const QByteArray &data);
    void sendGDS(uint8_t flagsHi, uint8_t opcode, const QByteArray &payload);

    // Configuration
    void setDeviceName(const QString &deviceName);
    QString deviceName() const { return m_deviceName; }

    // Terminal type sent in TERMINAL_TYPE negotiation (e.g. "IBM-3179-2")
    void setTerminalType(const QString &type) { m_terminalType = type; }
    QString terminalType() const { return m_terminalType; }

    // Credentials for IBMRSEED password encryption (RFC 4777)
    void setCredentials(const QString &username, const QString &password);
    QString username() const { return m_username; }
    QString password() const { return m_password; }

    // EBCDIC code page for this session (used in NEW_ENVIRON and password encryption)
    void setCodePage(core::CodePage::ID cp) { m_codePage = cp; }
    core::CodePage::ID codePage() const { return m_codePage; }

    // When true, SSL/TLS certificate validation errors (self-signed, expired,
    // hostname mismatch, untrusted chain) are ignored for this session. When
    // false (default), any SSL error aborts the connection before data flows.
    void setAllowInvalidCertificates(bool allow) { m_allowInvalidCertificates = allow; }
    bool allowInvalidCertificates() const { return m_allowInvalidCertificates; }

  signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);
    void stateChanged(ConnectionState state);

  private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
#ifdef HAVE_QT6_SSL
    void onSslErrors(const QList<QSslError> &errors);
#endif

  private:
    // State management
    void setState(ConnectionState newState);

    // Telnet negotiation
    void processTelnetData(const QByteArray &data);
    void sendTelnetCommand(telnet::TelnetCommand cmd, telnet::TelnetOption opt);
    void handleTelnetCommand(uint8_t cmd, uint8_t opt);
    void handleSubnegotiation(telnet::TelnetOption opt, const QByteArray &data);

    // TN5250 handshake
    void performHandshake();
    void sendDeviceName();
    void sendNewEnviron();
    void processHandshakeData(const QByteArray &data);
    void checkHandshakeComplete();

    // TLS negotiation
    void startTLS();
    void handleStartTLS();

    bool m_allowInvalidCertificates = false;
    QAbstractSocket *m_socket;
#ifdef HAVE_QT6_SSL
    QSslSocket *m_sslSocket;
#else
    void *m_sslSocket; // Placeholder when SSL not available
#endif
    QTcpSocket *m_tcpSocket;
    bool m_useTLS;
    bool m_tlsStarted;

    ConnectionState m_state;
    QString m_hostname;
    quint16 m_port;
    QString m_deviceName;
    bool m_deviceNameSent = false; // Track if we already sent DEVNAME
    QString m_terminalType = QStringLiteral("IBM-3179-2");
    QString m_username;
    QString m_password;
    core::CodePage::ID m_codePage = core::CodePage::ID::CP037;

    // IBMRSEED state (RFC 4777)
    QByteArray m_serverSeed;  // 8-byte seed from server's NEW_ENVIRON SEND

    QByteArray m_receiveBuffer;
    bool m_inSubnegotiation;
    telnet::TelnetOption m_currentSubnegotiation;
    QByteArray m_subnegotiationBuffer;

    bool m_handshakeComplete;
    QByteArray m_handshakeBuffer;
    bool m_binaryNegotiated;
    bool m_eorNegotiated;
    bool m_terminalTypeSent;

    // Keep-alive heartbeat
    QTimer *m_heartbeatTimer = nullptr;
    void startHeartbeat();
    void stopHeartbeat();
    void sendHeartbeat();
};

} // namespace tn5250::client
