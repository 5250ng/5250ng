#pragma once

#include "network/tn5250/telnet/commands.h"
#include "network/tn5250/telnet/options.h"
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>
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

    // Configuration
    void setDeviceName(const QString &deviceName);
    QString deviceName() const { return m_deviceName; }

    // Credentials for IBMRSEED password encryption (RFC 4777)
    void setCredentials(const QString &username, const QString &password);
    QString username() const { return m_username; }
    QString password() const { return m_password; }

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
    QString m_username;
    QString m_password;

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
};

} // namespace tn5250::client
