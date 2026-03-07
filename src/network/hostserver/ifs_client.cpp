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

#include "ifs_client.h"
#include "host_data_stream.h"
#include "password_encrypt.h"
#include "logger/logger.h"
#include <QTimer>

namespace hostserver {

IFSClient::IFSClient(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_state(State::Disconnected)
    , m_port(ports::FILE)
    , m_passwordLevel(0)
    , m_correlationCounter(0)
    , m_datastreamLevel(0)
    , m_maxDataBlockSize(0)
    , m_exchangeAttrsSent(false) {}

IFSClient::~IFSClient() {
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

void IFSClient::setState(State newState) {
    if (m_state == newState) return;
    m_state = newState;
    emit stateChanged(m_state);
}

void IFSClient::connectToHost(const QString &hostname,
                               const QString &userId, const QString &password,
                               uint8_t passwordLevel) {
    connectToHost(hostname, ports::FILE, userId, password, passwordLevel);
}

void IFSClient::connectToHost(const QString &hostname, uint16_t port,
                               const QString &userId, const QString &password,
                               uint8_t passwordLevel) {
    m_hostname = hostname;
    m_port = port;
    m_userId = userId;
    m_password = password;
    m_passwordLevel = passwordLevel;
    m_recvBuffer.clear();
    m_correlationCounter = 0;

    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
    }
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &IFSClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &IFSClient::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &IFSClient::onSocketError);
    connect(m_socket, &QTcpSocket::disconnected, this, &IFSClient::onSocketDisconnected);

    setState(State::Connecting);
    LOG_DEBUG(QString("[IFSClient]: Connecting to %1:%2").arg(hostname).arg(port));
    m_socket->connectToHost(hostname, port);
}

void IFSClient::disconnectFromHost() {
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
    setState(State::Disconnected);
}

uint32_t IFSClient::nextCorrelation() {
    return ++m_correlationCounter;
}

QByteArray IFSClient::buildIFSPacket(uint16_t templateLength, uint16_t reqRepId,
                                      const QByteArray &payload) {
    return HostDataStream::buildPacket(
        0x00, 0x00,
        ServerID::File,
        nextCorrelation(),
        templateLength,
        reqRepId,
        payload);
}

void IFSClient::sendPacket(const QByteArray &packet) {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        LOG_WARNING("[IFSClient]: Cannot send, not connected");
        return;
    }
    m_socket->write(packet);
    m_socket->flush();
}

QDateTime IFSClient::fromIBMTimestamp(uint64_t msEpoch) {
    if (msEpoch == 0) return {};
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(msEpoch), Qt::UTC);
}

// ---- Socket event handlers ----

void IFSClient::onSocketConnected() {
    LOG_DEBUG("[IFSClient]: Connected, starting authentication");
    setState(State::Authenticating);
    sendExchangeSeeds();
}

void IFSClient::onSocketReadyRead() {
    m_recvBuffer.append(m_socket->readAll());

    while (m_recvBuffer.size() >= 4) {
        uint32_t pktLen = HostDataStream::packetLength(m_recvBuffer);
        if (pktLen < HostDataStream::HEADER_SIZE || pktLen > 4 * 1024 * 1024) {
            setState(State::Error);
            emit errorOccurred(QString("Invalid packet length: %1").arg(pktLen));
            return;
        }
        if (m_recvBuffer.size() < static_cast<int>(pktLen)) {
            return; // Wait for more data
        }

        QByteArray packet = m_recvBuffer.left(static_cast<int>(pktLen));
        m_recvBuffer.remove(0, static_cast<int>(pktLen));
        dispatchPacket(packet);
    }
}

void IFSClient::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    QString msg = m_socket ? m_socket->errorString() : QStringLiteral("Unknown");
    LOG_ERROR(QString("[IFSClient]: Socket error: %1").arg(msg));
    setState(State::Error);
    emit errorOccurred(msg);
}

void IFSClient::onSocketDisconnected() {
    if (m_state != State::Disconnected && m_state != State::Error) {
        LOG_WARNING("[IFSClient]: Disconnected unexpectedly");
        setState(State::Disconnected);
        emit disconnected();
    }
}

// ---- Authentication ----

void IFSClient::sendExchangeSeeds() {
    m_clientSeed = PasswordEncrypt::generateSeed();

    // Exchange seeds: 28 bytes total
    // Header with clientAttr=0x01, templateLength=8, reqRepId=0x7001
    // Template = 8-byte client seed
    QByteArray packet = HostDataStream::buildPacket(
        0x01,               // client attributes
        0x00,
        ServerID::File,
        0x00000000,
        8,                  // template length = 8 (client seed)
        reqrep::EXCHANGE_SEEDS,
        m_clientSeed);      // payload = client seed (the template data)

    sendPacket(packet);
    LOG_DEBUG("[IFSClient]: Sent exchange seeds request");
}

void IFSClient::handleExchangeSeedsReply(const QByteArray &packet) {
    if (packet.size() < 32) {
        setState(State::Error);
        emit errorOccurred("Exchange seeds reply too short");
        return;
    }

    uint32_t returnCode = HostDataStream::readU32(packet, 20);
    if (returnCode != 0) {
        setState(State::Error);
        emit errorOccurred(QString("Exchange seeds failed (rc=0x%1)")
                               .arg(returnCode, 8, 16, QLatin1Char('0')));
        return;
    }

    // Server seed at offset 24, 8 bytes
    m_serverSeed = packet.mid(24, 8);

    // Server attributes at offset 5 may indicate encryption type
    LOG_DEBUG(QString("[IFSClient]: Got server seed (%1 bytes)")
                  .arg(m_serverSeed.size()));

    sendStartServer();
}

void IFSClient::sendStartServer() {
    QByteArray encryptedPw = PasswordEncrypt::encrypt(
        m_userId, m_password, m_clientSeed, m_serverSeed, m_passwordLevel);

    if (encryptedPw.isEmpty()) {
        setState(State::Error);
        emit errorOccurred("Password encryption failed");
        return;
    }

    uint8_t authScheme = PasswordEncrypt::authScheme(m_passwordLevel);

    // Template: 2 bytes (auth scheme + send reply flag)
    QByteArray templateData;
    templateData.append(static_cast<char>(authScheme));
    templateData.append(static_cast<char>(0x01)); // send reply = yes

    // Variable fields
    QByteArray varFields;
    varFields.append(HostDataStream::buildLLCP(codepoint::ENCRYPTED_PASSWORD, encryptedPw));

    QByteArray userIdEbcdic = HostDataStream::encodeUserId(m_userId);
    varFields.append(HostDataStream::buildLLCP(codepoint::USER_ID, userIdEbcdic));

    QByteArray payload;
    payload.append(templateData);
    payload.append(varFields);

    QByteArray packet = HostDataStream::buildPacket(
        0x02,               // client attributes for start server
        0x00,
        ServerID::File,
        0x00000000,
        2,                  // template length = 2
        reqrep::START_SERVER,
        payload);

    sendPacket(packet);
    LOG_DEBUG("[IFSClient]: Sent start server request");
}

void IFSClient::handleStartServerReply(const QByteArray &packet) {
    if (packet.size() < 24) {
        setState(State::Error);
        emit errorOccurred("Start server reply too short");
        return;
    }

    uint32_t returnCode = HostDataStream::readU32(packet, 20);
    if (returnCode != 0) {
        setState(State::Error);
        emit errorOccurred(QString("Start server failed (rc=0x%1)")
                               .arg(returnCode, 8, 16, QLatin1Char('0')));
        return;
    }

    LOG_DEBUG("[IFSClient]: Start server OK, will send exchange attributes");
    m_exchangeAttrsSent = false;
    setState(State::ExchangingAttributes);
    // Defer to next event loop iteration so any post-auth server notification
    // (0x8001) still in the TCP buffer is processed first.
    QTimer::singleShot(0, this, &IFSClient::sendIFSExchangeAttributes);
}

// ---- IFS attribute exchange ----

void IFSClient::sendIFSExchangeAttributes() {
    m_exchangeAttrsSent = true;
    // Template per JTOpen IFSExchangeAttrReq - NO chain indicator for this request:
    //   datastreamLevel(2) + dontConvertTable(2) + usePOSIXReturnCodes(2) +
    //   preferredCCSID(4) + maxDataBlockSize(4) = 14 bytes
    // No variable LL/CP fields.
    QByteArray templateData;
    HostDataStream::writeU16(templateData, 16);           // preferred datastream level
    HostDataStream::writeU16(templateData, 0x0001);       // don't convert table = 1
    HostDataStream::writeU16(templateData, 0x0001);       // use POSIX-style return codes = 1
    HostDataStream::writeU32(templateData, 1200);          // preferred CCSID (UTF-16BE)
    HostDataStream::writeU32(templateData, 0x000FC000);    // max data block ~1MB

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::EXCHANGE_ATTR,
        templateData);  // template IS the entire payload (no variable fields)

    sendPacket(packet);
    LOG_DEBUG(QString("[IFSClient]: Sent IFS exchange attributes (%1 bytes)")
                  .arg(packet.size()));
}

void IFSClient::handleIFSExchangeAttributesReply(const QByteArray &packet) {
    if (packet.size() < 30) {
        setState(State::Error);
        emit errorOccurred("IFS exchange attributes reply too short");
        return;
    }

    // Offset 22-23: server datastream level
    m_datastreamLevel = HostDataStream::readU16(packet, 22);
    // Offset 26-29: max data block size
    m_maxDataBlockSize = HostDataStream::readU32(packet, 26);

    LOG_INFO(QString("[IFSClient]: Ready (datastreamLevel=%1, maxBlock=%2)")
                 .arg(m_datastreamLevel)
                 .arg(m_maxDataBlockSize));

    setState(State::Ready);
    emit connected();
}

// ---- Packet dispatch ----

void IFSClient::dispatchPacket(const QByteArray &packet) {
    if (packet.size() < HostDataStream::HEADER_SIZE) return;

    uint16_t reqRepId = HostDataStream::readU16(packet, 18);

    // 0x8001: Server notification / generic error reply (seen on V5R4).
    // This is NOT a standard IFS operation reply - handle it globally before
    // state-specific dispatch to prevent it from being misinterpreted.
    if (reqRepId == 0x8001) {
        uint16_t rc = 0;
        if (packet.size() >= 24) {
            rc = HostDataStream::readU16(packet, 22); // chain(2) + returnCode(2)
        }
        LOG_DEBUG(QString("[IFSClient]: Server notification 0x8001 (rc=%1, state=%2)")
                      .arg(rc)
                      .arg(static_cast<int>(m_state)));

        if (m_state == State::ExchangingAttributes) {
            if (!m_exchangeAttrsSent) {
                // Post-auth notification arrived before we sent exchange attrs.
                // Skip it - the real exchange attrs reply will come later.
                LOG_DEBUG("[IFSClient]: Skipping post-auth notification (exchange attrs not sent yet)");
                return;
            }
            // Exchange attrs was sent but server replied with 0x8001 instead
            // of 0x8016 - server doesn't support it. Use safe defaults.
            m_datastreamLevel = 0;
            m_maxDataBlockSize = 0xF000; // 60 KB conservative default
            LOG_INFO(QString("[IFSClient]: Ready with defaults "
                             "(datastreamLevel=%1, maxBlock=%2)")
                         .arg(m_datastreamLevel)
                         .arg(m_maxDataBlockSize));
            setState(State::Ready);
            emit connected();
        }
        // In all other states, just log and skip - it's a server notification
        return;
    }

    // During authentication phase, handle auth-specific replies
    if (m_state == State::Authenticating) {
        if (reqRepId == reqrep::EXCHANGE_SEEDS || reqRepId == (reqrep::EXCHANGE_SEEDS | 0x8000)) {
            handleExchangeSeedsReply(packet);
            return;
        }
        if (reqRepId == reqrep::START_SERVER || reqRepId == (reqrep::START_SERVER | 0x8000)) {
            handleStartServerReply(packet);
            return;
        }
    }

    if (m_state == State::ExchangingAttributes) {
        if (reqRepId == ifs::EXCHANGE_ATTR_REPLY) {
            handleIFSExchangeAttributesReply(packet);
            return;
        }
        // Unexpected reply during exchange attributes - use safe defaults
        LOG_WARNING(QString("[IFSClient]: Exchange attributes got unexpected reply "
                            "0x%1 (%2 bytes), using defaults")
                        .arg(reqRepId, 4, 16, QLatin1Char('0'))
                        .arg(packet.size()));
        m_datastreamLevel = 0;
        m_maxDataBlockSize = 0xF000;
        setState(State::Ready);
        emit connected();
        return;
    }

    // IFS operation replies
    switch (reqRepId) {
    case ifs::LIST_ATTRS_REPLY:
        handleListAttrsReply(packet);
        break;
    case ifs::OPEN_FILE_REPLY:
        handleOpenFileReply(packet);
        break;
    case ifs::READ_FILE_REPLY:
        handleReadFileReply(packet);
        break;
    case ifs::RETURN_CODE_REPLY:
        handleReturnCodeReply(packet);
        break;
    default:
        LOG_DEBUG(QString("[IFSClient]: Unhandled reply 0x%1 (%2 bytes)")
                      .arg(reqRepId, 4, 16, QLatin1Char('0'))
                      .arg(packet.size()));
        break;
    }
}

// ---- File operations ----

void IFSClient::listDirectory(const QString &pattern) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    m_listBuffer.clear();

    // Template per JTOpen IFSListAttrsReq:
    //   chain(2) + fileHandle(4) + ccsid(2) + workingDirHandle(4) +
    //   checkAuthority(2) + maxGetCount(2) + patternMatching(2)  = 18 bytes
    //   + attrListLevel(2) if DSL >= 2                           = 20 bytes
    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000); // chain = 0
    HostDataStream::writeU32(templateData, 0);       // file handle = 0 (not used)
    HostDataStream::writeU16(templateData, 1200);    // CCSID = UTF-16BE
    HostDataStream::writeU32(templateData, 1);       // working dir handle = 1 (root)
    HostDataStream::writeU16(templateData, 0);       // check authority = 0
    HostDataStream::writeU16(templateData, 0xFFFF);  // max get count = all
    HostDataStream::writeU16(templateData, 0x0001);  // pattern matching: POSIX_ALL
    if (m_datastreamLevel >= 2) {
        HostDataStream::writeU16(templateData, 0x0001); // attr list level: basic
    }

    // Filename/pattern as UTF-16BE
    QByteArray filenameUtf16 = HostDataStream::toUtf16BE(pattern);
    QByteArray filenameField = HostDataStream::buildLLCP(codepoint::IFS_FILENAME, filenameUtf16);

    QByteArray payload;
    payload.append(templateData);
    payload.append(filenameField);

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::LIST_ATTRS,
        payload);

    setState(State::Busy);
    sendPacket(packet);
    LOG_DEBUG(QString("[IFSClient]: List directory: %1").arg(pattern));
}

void IFSClient::openFile(const QString &path, uint16_t accessIntent,
                          uint16_t shareMode, uint16_t dupFileOption) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    bool useLargeOffsets = (m_datastreamLevel >= 16);

    // Template varies by DSL
    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000); // chain = 0
    HostDataStream::writeU16(templateData, 1200);    // filename CCSID
    HostDataStream::writeU32(templateData, 1);       // working dir handle
    HostDataStream::writeU16(templateData, 0);       // file data CCSID (binary)
    HostDataStream::writeU16(templateData, accessIntent);
    HostDataStream::writeU16(templateData, shareMode);
    HostDataStream::writeU16(templateData, 0);       // data conversion = none
    HostDataStream::writeU16(templateData, dupFileOption);

    if (useLargeOffsets) {
        HostDataStream::writeU32(templateData, 0);   // create size (ignored, use large)
        HostDataStream::writeU32(templateData, 0);   // fixed attributes
        HostDataStream::writeU16(templateData, 1);   // attr list level
        HostDataStream::writeU32(templateData, 0);   // pre-read offset
        HostDataStream::writeU32(templateData, 0);   // pre-read length
        HostDataStream::writeU64(templateData, 0);   // large create size
    } else {
        HostDataStream::writeU32(templateData, 0);   // create size
        HostDataStream::writeU32(templateData, 0);   // fixed attributes
        HostDataStream::writeU16(templateData, 1);   // attr list level
        HostDataStream::writeU32(templateData, 0);   // pre-read offset
        HostDataStream::writeU32(templateData, 0);   // pre-read length
    }

    // Filename
    QByteArray filenameUtf16 = HostDataStream::toUtf16BE(path);
    QByteArray filenameField = HostDataStream::buildLLCP(codepoint::IFS_FILENAME, filenameUtf16);

    QByteArray payload;
    payload.append(templateData);
    payload.append(filenameField);

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::OPEN_FILE,
        payload);

    setState(State::Busy);
    sendPacket(packet);
    LOG_DEBUG(QString("[IFSClient]: Open file: %1 (access=0x%2)")
                  .arg(path)
                  .arg(accessIntent, 4, 16, QLatin1Char('0')));
}

void IFSClient::openFileForRead(const QString &path) {
    openFile(path, ifs_access::READ_ACCESS, ifs_share::DENY_NONE,
             ifs_dupopt::FAIL_OPEN);
}

void IFSClient::openFileForWrite(const QString &path) {
    openFile(path, ifs_access::WRITE_ACCESS, ifs_share::DENY_WRITERS,
             ifs_dupopt::CREATE_OR_REPLACE);
}

void IFSClient::readFile(uint32_t fileHandle, uint64_t offset, uint32_t length) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    bool useLargeOffsets = (m_datastreamLevel >= 16);

    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000); // chain = 0
    HostDataStream::writeU32(templateData, fileHandle);

    if (useLargeOffsets) {
        HostDataStream::writeU32(templateData, 0);       // base offset = 0
        HostDataStream::writeU32(templateData, 0);       // relative offset (unused for large)
        HostDataStream::writeU32(templateData, length);   // read length
        HostDataStream::writeU32(templateData, 0);       // pre-read length
        HostDataStream::writeU64(templateData, 0);       // large base offset
        HostDataStream::writeU64(templateData, offset);  // large relative offset
    } else {
        HostDataStream::writeU32(templateData, 0);       // base offset
        HostDataStream::writeU32(templateData, static_cast<uint32_t>(offset)); // relative offset
        HostDataStream::writeU32(templateData, length);
        HostDataStream::writeU32(templateData, 0);       // pre-read length
    }

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::READ_FILE,
        templateData); // template IS the payload here (no variable fields)

    setState(State::Busy);
    sendPacket(packet);
}

void IFSClient::writeFile(uint32_t fileHandle, uint64_t offset, const QByteArray &data) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    bool useLargeOffsets = (m_datastreamLevel >= 16);

    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000); // chain = 0
    HostDataStream::writeU32(templateData, fileHandle);

    if (useLargeOffsets) {
        HostDataStream::writeU32(templateData, 0);       // base offset
        HostDataStream::writeU32(templateData, 0);       // relative offset (unused)
        HostDataStream::writeU16(templateData, 0x0002);  // data flags = normal
        HostDataStream::writeU16(templateData, 0);       // CCSID (binary)
        HostDataStream::writeU64(templateData, 0);       // large base offset
        HostDataStream::writeU64(templateData, offset);  // large relative offset
    } else {
        HostDataStream::writeU32(templateData, 0);
        HostDataStream::writeU32(templateData, static_cast<uint32_t>(offset));
        HostDataStream::writeU16(templateData, 0x0002);
        HostDataStream::writeU16(templateData, 0);
    }

    // File data as LL/CP block
    QByteArray dataField = HostDataStream::buildLLCP(codepoint::IFS_FILE_DATA, data);

    QByteArray payload;
    payload.append(templateData);
    payload.append(dataField);

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::WRITE_FILE,
        payload);

    setState(State::Busy);
    sendPacket(packet);
}

void IFSClient::closeFile(uint32_t fileHandle) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    // Template: chain(2) + handle(4) + dataFlags(2) + ccsid(2) +
    //           amountAccessed(2) + accessHistory(1) + modifyDate(8) = 21
    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000); // chain
    HostDataStream::writeU32(templateData, fileHandle);
    HostDataStream::writeU16(templateData, 0x0002); // data flags
    HostDataStream::writeU16(templateData, 0xFFFF); // CCSID
    HostDataStream::writeU16(templateData, 100);    // amount accessed
    templateData.append(static_cast<char>(0));       // access history
    HostDataStream::writeU64(templateData, 0);       // modify date (0 = don't change)

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::CLOSE_FILE,
        templateData);

    sendPacket(packet);
    LOG_DEBUG(QString("[IFSClient]: Close file handle %1").arg(fileHandle));
}

void IFSClient::deleteFile(const QString &path) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    // Template: chain(2) + ccsid(2) + workingDirHandle(4)
    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000);
    HostDataStream::writeU16(templateData, 1200);
    HostDataStream::writeU32(templateData, 1);

    QByteArray filenameUtf16 = HostDataStream::toUtf16BE(path);
    QByteArray filenameField = HostDataStream::buildLLCP(codepoint::IFS_FILENAME, filenameUtf16);

    QByteArray payload;
    payload.append(templateData);
    payload.append(filenameField);

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::DELETE_FILE,
        payload);

    setState(State::Busy);
    sendPacket(packet);
    LOG_DEBUG(QString("[IFSClient]: Delete file: %1").arg(path));
}

void IFSClient::createDirectory(const QString &path) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    // Template: chain(2) + ccsid(2) + workingDirHandle(4)
    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000);
    HostDataStream::writeU16(templateData, 1200);
    HostDataStream::writeU32(templateData, 1);

    QByteArray nameUtf16 = HostDataStream::toUtf16BE(path);
    QByteArray nameField = HostDataStream::buildLLCP(codepoint::IFS_DIR_NAME, nameUtf16);

    QByteArray payload;
    payload.append(templateData);
    payload.append(nameField);

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::CREATE_DIR,
        payload);

    setState(State::Busy);
    sendPacket(packet);
    LOG_DEBUG(QString("[IFSClient]: Create directory: %1").arg(path));
}

void IFSClient::deleteDirectory(const QString &path) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    // Template: chain(2) + ccsid(2) + workingDirHandle(4) + flags(2)
    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000);
    HostDataStream::writeU16(templateData, 1200);
    HostDataStream::writeU32(templateData, 1);
    HostDataStream::writeU16(templateData, 0x0000); // flags

    QByteArray nameUtf16 = HostDataStream::toUtf16BE(path);
    QByteArray nameField = HostDataStream::buildLLCP(codepoint::IFS_DIR_NAME, nameUtf16);

    QByteArray payload;
    payload.append(templateData);
    payload.append(nameField);

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::DELETE_DIR,
        payload);

    setState(State::Busy);
    sendPacket(packet);
    LOG_DEBUG(QString("[IFSClient]: Delete directory: %1").arg(path));
}

void IFSClient::rename(const QString &oldPath, const QString &newPath) {
    if (m_state != State::Ready && m_state != State::Busy) {
        emit errorOccurred("Not connected");
        return;
    }

    // Template: chain(2) + sourceCCSID(2) + targetCCSID(2) +
    //           sourceWorkDir(4) + targetWorkDir(4) + flags(2) = 16
    QByteArray templateData;
    HostDataStream::writeU16(templateData, 0x0000);  // chain
    HostDataStream::writeU16(templateData, 1200);     // source CCSID
    HostDataStream::writeU16(templateData, 1200);     // target CCSID
    HostDataStream::writeU32(templateData, 1);        // source working dir
    HostDataStream::writeU32(templateData, 1);        // target working dir
    HostDataStream::writeU16(templateData, 0x0001);   // flags: allow replace

    QByteArray sourceUtf16 = HostDataStream::toUtf16BE(oldPath);
    QByteArray sourceField = HostDataStream::buildLLCP(codepoint::IFS_RENAME_SOURCE, sourceUtf16);

    QByteArray targetUtf16 = HostDataStream::toUtf16BE(newPath);
    QByteArray targetField = HostDataStream::buildLLCP(codepoint::IFS_RENAME_TARGET, targetUtf16);

    QByteArray payload;
    payload.append(templateData);
    payload.append(sourceField);
    payload.append(targetField);

    QByteArray packet = buildIFSPacket(
        static_cast<uint16_t>(templateData.size()),
        ifs::RENAME,
        payload);

    setState(State::Busy);
    sendPacket(packet);
    LOG_DEBUG(QString("[IFSClient]: Rename: %1 -> %2").arg(oldPath, newPath));
}

// ---- Reply handlers ----

void IFSClient::handleListAttrsReply(const QByteArray &packet) {
    // Each reply is one directory entry.
    // Template format depends on datastream level and attrListLevel requested.
    uint16_t templateLen = HostDataStream::readU16(packet, 16);

    if (packet.size() < 20 + templateLen) {
        LOG_WARNING("[IFSClient]: List attrs reply too short");
        return;
    }

    IFSDirectoryEntry entry;

    // Template starts at offset 20. First 2 bytes are chain indicator.
    int base = 20;

    // For DSL >= 2 (attrListLevel sent): 8-byte timestamps, template >= 61 bytes
    //   chain(2) + createDate(8) + modDate(8) + accessDate(8) + fileSize(4) +
    //   fixedAttrs(4) + objectType(2) + ... = 36+ bytes before extended fields
    // For DSL < 2 (no attrListLevel): 4-byte timestamps, shorter template
    //   chain(2) + createDate(4) + modDate(4) + accessDate(4) + fileSize(4) +
    //   fixedAttrs(4) + objectType(2) + ... = 24+ bytes

    if (templateLen >= 36) {
        // New format: 8-byte timestamps (DSL >= 2 or attrListLevel was sent)
        entry.createDate = fromIBMTimestamp(HostDataStream::readU64(packet, base + 2));
        entry.modifyDate = fromIBMTimestamp(HostDataStream::readU64(packet, base + 10));
        entry.accessDate = fromIBMTimestamp(HostDataStream::readU64(packet, base + 18));
        uint32_t fileSize32 = HostDataStream::readU32(packet, base + 26);
        entry.fixedAttributes = HostDataStream::readU32(packet, base + 30);
        entry.objectType = HostDataStream::readU16(packet, base + 34);

        // Large file size at base+61 if template is long enough (DSL >= 8)
        if (templateLen >= 71) {
            entry.fileSize = HostDataStream::readU64(packet, base + 61);
        } else {
            entry.fileSize = fileSize32;
        }

        // Symbolic link flag at base+71 if available
        if (templateLen >= 72) {
            entry.isSymlink = (static_cast<uint8_t>(packet[base + 71]) != 0);
        }
    } else if (templateLen >= 24) {
        // Old format: 4-byte timestamps (DSL 0, no attrListLevel)
        // V5R4 uses unsigned 32-bit values for dates (seconds since 1970?)
        uint32_t createSec = HostDataStream::readU32(packet, base + 2);
        uint32_t modSec = HostDataStream::readU32(packet, base + 6);
        uint32_t accessSec = HostDataStream::readU32(packet, base + 10);
        entry.createDate = QDateTime::fromSecsSinceEpoch(createSec, Qt::UTC);
        entry.modifyDate = QDateTime::fromSecsSinceEpoch(modSec, Qt::UTC);
        entry.accessDate = QDateTime::fromSecsSinceEpoch(accessSec, Qt::UTC);
        entry.fileSize = HostDataStream::readU32(packet, base + 14);
        entry.fixedAttributes = HostDataStream::readU32(packet, base + 18);
        entry.objectType = HostDataStream::readU16(packet, base + 22);
    } else {
        LOG_WARNING(QString("[IFSClient]: List attrs reply template too short (%1 bytes)")
                        .arg(templateLen));
        return;
    }

    // Variable-length fields follow the template
    int varOffset = 20 + templateLen;

    // Find filename (CP 0x0002)
    QByteArray nameData = HostDataStream::findCodePoint(packet, varOffset, codepoint::IFS_FILENAME);
    if (!nameData.isEmpty()) {
        entry.name = HostDataStream::fromUtf16BE(nameData);
    }

    m_listBuffer.append(entry);

    // Check chain indicator (offset 20-21): if bit 0 is set, more entries follow
    uint16_t chain = HostDataStream::readU16(packet, 20);
    if ((chain & 0x0001) == 0) {
        // Last entry in chain - emit the complete listing
        QVector<IFSDirectoryEntry> result = m_listBuffer;
        m_listBuffer.clear();
        if (m_state == State::Busy) setState(State::Ready);
        emit directoryListed(result);
    }
}

void IFSClient::handleOpenFileReply(const QByteArray &packet) {
    if (packet.size() < 72) {
        if (m_state == State::Busy) setState(State::Ready);
        emit errorOccurred("Open file reply too short");
        return;
    }

    IFSFileHandle fh;
    fh.handle = HostDataStream::readU32(packet, 22);
    fh.ccsid = HostDataStream::readU16(packet, 34);
    fh.createDate = fromIBMTimestamp(HostDataStream::readU64(packet, 38));
    fh.modifyDate = fromIBMTimestamp(HostDataStream::readU64(packet, 46));

    // File size: 4-byte at offset 62 for DSL<16, 8-byte at 89 for DSL>=16
    if (m_datastreamLevel >= 16 && packet.size() >= 97) {
        fh.fileSize = HostDataStream::readU64(packet, 89);
    } else {
        fh.fileSize = HostDataStream::readU32(packet, 62);
    }

    fh.valid = true;

    LOG_DEBUG(QString("[IFSClient]: File opened, handle=%1, size=%2")
                  .arg(fh.handle)
                  .arg(fh.fileSize));

    if (m_state == State::Busy) setState(State::Ready);
    emit fileOpened(fh);
}

void IFSClient::handleReadFileReply(const QByteArray &packet) {
    // Template: chain(2) + fileHandle(4) + ccsid(2) + dataLength(4) => 12 bytes
    // Then LL/CP data block follows
    if (packet.size() < 32) {
        if (m_state == State::Busy) setState(State::Ready);
        emit errorOccurred("Read file reply too short");
        return;
    }

    // File handle at offset 22 (after chain indicator at 20)
    uint32_t fileHandle = HostDataStream::readU32(packet, 22);

    // Data LL/CP starts after the template
    uint16_t templateLen = HostDataStream::readU16(packet, 16);
    int dataStart = 20 + templateLen;

    QByteArray fileData;
    if (dataStart + 6 <= packet.size()) {
        uint32_t dataLL = HostDataStream::readU32(packet, dataStart);
        uint16_t dataCP = HostDataStream::readU16(packet, dataStart + 4);

        if (dataCP == codepoint::IFS_FILE_DATA && dataLL > 6) {
            int dataLen = static_cast<int>(dataLL) - 6;
            if (dataStart + 6 + dataLen <= packet.size()) {
                fileData = packet.mid(dataStart + 6, dataLen);
            }
        }
    }

    if (m_state == State::Busy) setState(State::Ready);
    emit fileDataRead(fileHandle, fileData);
}

void IFSClient::handleReturnCodeReply(const QByteArray &packet) {
    if (packet.size() < 24) return;

    // Template: chain(2) + returnCode(2) => starts at offset 20
    uint16_t returnCode = HostDataStream::readU16(packet, 22);

    // Extract file handle from template if available (at offset 24, 4 bytes)
    uint32_t fileHandle = 0;
    if (packet.size() >= 28) {
        fileHandle = HostDataStream::readU32(packet, 24);
    }

    LOG_DEBUG(QString("[IFSClient]: Return code %1 (%2), handle=%3")
                  .arg(returnCode)
                  .arg(ifs_rc::toString(returnCode))
                  .arg(fileHandle));

    // Return code 0x8004 is also used for:
    // - close file acknowledgement
    // - write file acknowledgement
    // - delete/mkdir/rmdir/rename results
    // - NO_MORE_FILES at end of directory listing

    if (returnCode == ifs_rc::NO_MORE_FILES) {
        // End of directory listing chain
        QVector<IFSDirectoryEntry> result = m_listBuffer;
        m_listBuffer.clear();
        if (m_state == State::Busy) setState(State::Ready);
        emit directoryListed(result);
        return;
    }

    if (returnCode == ifs_rc::NO_MORE_DATA) {
        // EOF on read - emit empty data with the file handle
        if (m_state == State::Busy) setState(State::Ready);
        emit fileDataRead(fileHandle, QByteArray());
        return;
    }

    if (returnCode == ifs_rc::SUCCESS) {
        // Close file acknowledgement
        if (fileHandle != 0) {
            if (m_state == State::Busy) setState(State::Ready);
            emit fileClosed(fileHandle);
            return;
        }
    }

    if (m_state == State::Busy) setState(State::Ready);
    emit operationCompleted(HostDataStream::readU16(packet, 18), returnCode);
}

} // namespace hostserver
