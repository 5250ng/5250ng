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

#include "signon_client.h"
#include "core/codepage.h"
#include "host_data_stream.h"
#include "password_encrypt.h"
#include "logger/logger.h"

namespace hostserver {

SignonClient::SignonClient(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_state(State::Disconnected)
    , m_port(ports::SIGNON)
    , m_passwordLevel(0)
    , m_serverLevel(0)
    , m_serverVersion(0)
    , m_serverCCSID(0) {}

SignonClient::~SignonClient() {
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

void SignonClient::setState(State newState) {
    if (m_state == newState) return;
    m_state = newState;
    emit stateChanged(m_state);
}

void SignonClient::signon(const QString &hostname,
                          const QString &userId,
                          const QString &password) {
    signon(hostname, ports::SIGNON, userId, password);
}

void SignonClient::signon(const QString &hostname, uint16_t port,
                          const QString &userId, const QString &password) {
    m_hostname = hostname;
    m_port = port;
    m_userId = userId;
    m_password = password;
    m_recvBuffer.clear();

    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
    }
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &SignonClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &SignonClient::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &SignonClient::onSocketError);
    connect(m_socket, &QTcpSocket::disconnected, this, &SignonClient::onSocketDisconnected);

    setState(State::Connecting);
    LOG_DEBUG(QString("[SignonClient]: Connecting to %1:%2").arg(hostname).arg(port));
    m_socket->connectToHost(hostname, port);
}

void SignonClient::onSocketConnected() {
    LOG_DEBUG("[SignonClient]: Connected, sending exchange attributes");
    setState(State::ExchangingAttributes);
    sendExchangeAttributes();
}

void SignonClient::onSocketReadyRead() {
    m_recvBuffer.append(m_socket->readAll());

    // Need at least 4 bytes for packet length
    while (m_recvBuffer.size() >= 4) {
        uint32_t pktLen = HostDataStream::packetLength(m_recvBuffer);
        if (pktLen < HostDataStream::HEADER_SIZE ||
            pktLen > 65536) {
            setState(State::Error);
            emit errorOccurred(QString("Invalid packet length: %1").arg(pktLen));
            return;
        }
        if (m_recvBuffer.size() < static_cast<int>(pktLen)) {
            return; // Wait for more data
        }

        QByteArray packet = m_recvBuffer.left(static_cast<int>(pktLen));
        m_recvBuffer.remove(0, static_cast<int>(pktLen));

        uint16_t reqRepId = HostDataStream::readU16(packet, 18);

        if (m_state == State::ExchangingAttributes && reqRepId == 0xF003) {
            handleExchangeAttributesReply(packet);
        } else if (m_state == State::Authenticating) {
            handleSignonReply(packet);
        } else {
            LOG_WARNING(QString("[SignonClient]: Unexpected packet reqRepId=0x%1 in state %2")
                            .arg(reqRepId, 4, 16, QLatin1Char('0'))
                            .arg(static_cast<int>(m_state)));
        }
    }
}

void SignonClient::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    QString msg = m_socket ? m_socket->errorString() : QStringLiteral("Unknown");
    LOG_ERROR(QString("[SignonClient]: Socket error: %1").arg(msg));
    setState(State::Error);
    emit errorOccurred(msg);
}

void SignonClient::onSocketDisconnected() {
    if (m_state != State::Authenticated && m_state != State::Error) {
        LOG_WARNING("[SignonClient]: Disconnected unexpectedly");
        setState(State::Error);
        emit errorOccurred("Connection closed unexpectedly");
    }
}

void SignonClient::sendExchangeAttributes() {
    // Generate client seed
    m_clientSeed = PasswordEncrypt::generateSeed();

    // Build payload: client version (CP 0x1101) + datastream level (CP 0x1102) + client seed (CP 0x1103)
    QByteArray payload;

    // Client version = 1
    QByteArray versionData;
    HostDataStream::writeU32(versionData, 1);
    payload.append(HostDataStream::buildLLCP(codepoint::CLIENT_VERSION, versionData));

    // Datastream level = 10 (for signon server)
    QByteArray dsLevelData;
    HostDataStream::writeU16(dsLevelData, 10);
    payload.append(HostDataStream::buildLLCP(codepoint::DATASTREAM_LEVEL, dsLevelData));

    // Client seed
    payload.append(HostDataStream::buildLLCP(codepoint::CLIENT_SEED, m_clientSeed));

    QByteArray packet = HostDataStream::buildPacket(
        0x00,                                   // client attributes
        0x00,                                   // server attributes
        ServerID::Signon,                       // server ID
        0x00000000,                             // correlation
        0x0000,                                 // template length
        reqrep::SIGNON_EXCHANGE_ATTR,           // request ID 0x7003
        payload);

    m_socket->write(packet);
    m_socket->flush();
    LOG_DEBUG(QString("[SignonClient]: Sent exchange attributes (%1 bytes)")
                  .arg(packet.size()));
}

void SignonClient::handleExchangeAttributesReply(const QByteArray &packet) {
    if (packet.size() < HostDataStream::HEADER_SIZE + 4) {
        setState(State::Error);
        emit errorOccurred("Exchange attributes reply too short");
        return;
    }

    // Template starts at offset 20; first 4 bytes are the return code
    uint32_t returnCode = HostDataStream::readU32(packet, 20);
    if (returnCode != 0) {
        setState(State::Error);
        emit errorOccurred(QString("Exchange attributes failed, return code: 0x%1")
                               .arg(returnCode, 8, 16, QLatin1Char('0')));
        return;
    }

    // Use the header's template length to find where variable fields start
    uint16_t templateLen = HostDataStream::readU16(packet, 16);
    int offset = 20 + templateLen;

    m_passwordLevel = 0;
    m_serverVersion = 0;
    m_serverLevel = 0;

    while (offset + 6 <= packet.size()) {
        uint32_t ll = HostDataStream::readU32(packet, offset);
        if (ll < 6 || static_cast<qint64>(ll) > packet.size() - offset) break;

        uint16_t cp = HostDataStream::readU16(packet, offset + 4);
        int dataOffset = offset + 6;
        int dataLen = static_cast<int>(ll - 6);

        switch (cp) {
        case codepoint::CLIENT_VERSION:
            if (dataLen >= 4) m_serverVersion = HostDataStream::readU32(packet, dataOffset);
            break;
        case codepoint::DATASTREAM_LEVEL:
            if (dataLen >= 2) m_serverLevel = HostDataStream::readU16(packet, dataOffset);
            break;
        case codepoint::SERVER_SEED:
            if (dataLen >= 8) m_serverSeed = packet.mid(dataOffset, 8);
            break;
        case codepoint::PASSWORD_LEVEL:
            if (dataLen >= 1) m_passwordLevel = static_cast<uint8_t>(packet[dataOffset]);
            break;
        case codepoint::JOB_NAME: {
            core::CodePage cp037(core::CodePage::ID::CP037);
            m_jobName = cp037.toUnicode(packet.mid(dataOffset, dataLen)).trimmed();
            break;
        }
        default:
            break;
        }

        offset += static_cast<int>(ll);
    }

    if (m_serverSeed.size() != 8) {
        setState(State::Error);
        emit errorOccurred("Server did not provide seed in exchange attributes reply");
        return;
    }

    LOG_DEBUG(QString("[SignonClient]: Exchange attributes OK, password level=%1, "
                      "server version=0x%2, server level=%3")
                  .arg(m_passwordLevel)
                  .arg(m_serverVersion, 8, 16, QLatin1Char('0'))
                  .arg(m_serverLevel));

    setState(State::Authenticating);
    sendSignonInfo();
}

void SignonClient::sendSignonInfo() {
    // Encrypt password based on password level
    QByteArray encryptedPw = PasswordEncrypt::encrypt(
        m_userId, m_password, m_clientSeed, m_serverSeed, m_passwordLevel);

    if (encryptedPw.isEmpty()) {
        setState(State::Error);
        emit errorOccurred("Password encryption failed");
        return;
    }

    uint8_t authScheme = PasswordEncrypt::authScheme(m_passwordLevel);

    // Build template: 1 byte auth scheme
    QByteArray templateData;
    templateData.append(static_cast<char>(authScheme));

    // Build variable fields (order matches JTOpen: userId, password, optional)
    QByteArray varFields;

    // User ID (10-byte EBCDIC) - required
    QByteArray userIdEbcdic = HostDataStream::encodeUserId(m_userId);
    varFields.append(HostDataStream::buildLLCP(codepoint::USER_ID, userIdEbcdic));

    // Encrypted password - required
    varFields.append(HostDataStream::buildLLCP(codepoint::ENCRYPTED_PASSWORD, encryptedPw));

    // Return error messages - only supported at server level >= 5
    if (m_serverLevel >= 5) {
        QByteArray errMsgFlag;
        errMsgFlag.append(static_cast<char>(0x01));
        varFields.append(HostDataStream::buildLLCP(codepoint::RETURN_ERROR_MSGS, errMsgFlag));
    }

    QByteArray payload;
    payload.append(templateData);
    payload.append(varFields);

    QByteArray packet = HostDataStream::buildPacket(
        0x00,
        0x00,
        ServerID::Signon,
        0x00000000,
        1,                                      // template length = 1 (auth scheme)
        reqrep::SIGNON_INFO,                    // request ID 0x7004
        payload);

    m_socket->write(packet);
    m_socket->flush();
    LOG_DEBUG(QString("[SignonClient]: Sent signon info (%1 bytes, auth scheme=0x%2, "
                      "pw=%3 bytes, serverLevel=%4)")
                  .arg(packet.size())
                  .arg(authScheme, 2, 16, QLatin1Char('0'))
                  .arg(encryptedPw.size())
                  .arg(m_serverLevel));
    LOG_DEBUG(QString("[SignonClient]: Packet hex: %1")
                  .arg(QString::fromLatin1(packet.toHex(' '))));
}

void SignonClient::handleSignonReply(const QByteArray &packet) {
    if (packet.size() < 24) {
        setState(State::Error);
        emit errorOccurred("Signon reply too short");
        return;
    }

    uint32_t returnCode = HostDataStream::readU32(packet, 20);
    if (returnCode != 0) {
        QString errorMsg;
        switch (returnCode) {
        // Category 0x0001: Request/data errors
        case 0x00010001: errorMsg = "Invalid request"; break;
        case 0x00010002: errorMsg = "User ID length not valid"; break;
        case 0x00010003: errorMsg = "Password length not valid"; break;
        case 0x00010004: errorMsg = "Authentication token not valid"; break;
        case 0x00010005: errorMsg = "Token type not valid"; break;
        case 0x00010007: errorMsg = "Password not specified"; break;
        case 0x00010008: errorMsg = "User ID not specified"; break;
        case 0x0001000A: errorMsg = "Request data error"; break;
        case 0x0001000B: errorMsg = "Request data field length error"; break;
        // Category 0x0002: User profile errors
        case 0x00020001: errorMsg = "Unknown user ID"; break;
        // Category 0x0003: Authentication errors
        case 0x00030001: errorMsg = "Incorrect password"; break;
        case 0x0003000B: errorMsg = "Incorrect password"; break;
        case 0x0003000C: errorMsg = "Password expired"; break;
        case 0x0003000D: errorMsg = "Password disabled"; break;
        case 0x00030010: errorMsg = "Password too long"; break;
        case 0x00030011: errorMsg = "Password too short"; break;
        default:
            errorMsg = QString("Signon failed (return code 0x%1)")
                           .arg(returnCode, 8, 16, QLatin1Char('0'));
            break;
        }
        LOG_ERROR(QString("[SignonClient]: %1").arg(errorMsg));
        setState(State::Error);
        emit errorOccurred(errorMsg);
        return;
    }

    // Parse optional code points from reply
    int offset = 24;
    while (offset + 6 <= packet.size()) {
        uint32_t ll = HostDataStream::readU32(packet, offset);
        if (ll < 6 || static_cast<qint64>(ll) > packet.size() - offset) break;

        uint16_t cp = HostDataStream::readU16(packet, offset + 4);
        int dataOffset = offset + 6;
        int dataLen = static_cast<int>(ll - 6);

        switch (cp) {
        case codepoint::SERVER_CCSID:
            if (dataLen >= 4) m_serverCCSID = HostDataStream::readU32(packet, dataOffset);
            break;
        case codepoint::JOB_NAME: {
            core::CodePage cp037(core::CodePage::ID::CP037);
            m_jobName = cp037.toUnicode(packet.mid(dataOffset, dataLen)).trimmed();
            break;
        }
        default:
            break;
        }
        offset += static_cast<int>(ll);
    }

    LOG_INFO(QString("[SignonClient]: Signon successful (user=%1, serverCCSID=%2)")
                 .arg(m_userId)
                 .arg(m_serverCCSID));

    setState(State::Authenticated);
    emit authenticated();
}

} // namespace hostserver
