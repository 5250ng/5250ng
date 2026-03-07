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
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtNetwork/QTcpSocket>
#include <cstdint>

namespace hostserver {

// Entry returned by directory listing operations
struct IFSDirectoryEntry {
    QString name;
    uint16_t objectType = 0;     // ifs_objtype::FILE, DIRECTORY, etc.
    uint64_t fileSize = 0;
    QDateTime createDate;
    QDateTime modifyDate;
    QDateTime accessDate;
    uint32_t fixedAttributes = 0;
    bool isSymlink = false;

    bool isDirectory() const {
        return objectType == ifs_objtype::DIRECTORY ||
               (fixedAttributes & ifs_attr::DIRECTORY);
    }
    bool isFile() const { return objectType == ifs_objtype::FILE; }
    bool isReadOnly() const { return fixedAttributes & ifs_attr::READ_ONLY; }
    bool isHidden() const { return fixedAttributes & ifs_attr::HIDDEN; }
};

// Returned by file open operations
struct IFSFileHandle {
    uint32_t handle = 0;
    uint64_t fileSize = 0;
    QDateTime createDate;
    QDateTime modifyDate;
    uint16_t ccsid = 0;
    bool valid = false;
};

// IBM i IFS (Integrated File System) client.
//
// Connects to the IFS file server (port 8473) and provides operations:
// - List directory contents
// - Open / read / write / close files
// - Create / delete directories
// - Delete / rename files
//
// Authentication is performed via the exchange seeds + start server
// flow using credentials from a prior signon.
class IFSClient : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Disconnected,
        Connecting,
        Authenticating,
        ExchangingAttributes,
        Ready,
        Busy,
        Error
    };

    explicit IFSClient(QObject *parent = nullptr);
    ~IFSClient();

    // Connect and authenticate to the IFS server.
    // userId/password: credentials (encrypted during handshake)
    // passwordLevel: from SignonClient::passwordLevel()
    void connectToHost(const QString &hostname, uint16_t port,
                       const QString &userId, const QString &password,
                       uint8_t passwordLevel);

    // Convenience: use default port
    void connectToHost(const QString &hostname,
                       const QString &userId, const QString &password,
                       uint8_t passwordLevel);

    void disconnectFromHost();
    State state() const { return m_state; }
    bool isReady() const { return m_state == State::Ready; }

    // ---- File operations (async, emit signals on completion) ----

    // List directory contents. Pattern is an IFS path with optional
    // wildcards, e.g. "/home/user/*" or "/QSYS.LIB/*.LIB"
    void listDirectory(const QString &pattern);

    // Open a file for reading or writing
    void openFile(const QString &path, uint16_t accessIntent, uint16_t shareMode,
                  uint16_t dupFileOption);

    // Convenience: open for reading
    void openFileForRead(const QString &path);

    // Convenience: open for writing (create or replace)
    void openFileForWrite(const QString &path);

    // Read data from an open file. Offset is from the beginning of the file.
    void readFile(uint32_t fileHandle, uint64_t offset, uint32_t length);

    // Write data to an open file at the given offset
    void writeFile(uint32_t fileHandle, uint64_t offset, const QByteArray &data);

    // Close an open file
    void closeFile(uint32_t fileHandle);

    // Delete a file
    void deleteFile(const QString &path);

    // Create a directory
    void createDirectory(const QString &path);

    // Delete a directory
    void deleteDirectory(const QString &path);

    // Rename a file or directory
    void rename(const QString &oldPath, const QString &newPath);

    // Max data block size negotiated with server
    uint32_t maxDataBlockSize() const { return m_maxDataBlockSize; }

  signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void stateChanged(State state);

    // Operation results
    void directoryListed(const QVector<IFSDirectoryEntry> &entries);
    void fileOpened(const IFSFileHandle &handle);
    void fileDataRead(uint32_t fileHandle, const QByteArray &data);
    void fileWritten(uint32_t fileHandle, uint32_t bytesWritten);
    void fileClosed(uint32_t fileHandle);
    void operationCompleted(uint16_t requestId, uint16_t returnCode);

  private slots:
    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketDisconnected();

  private:
    void setState(State newState);

    // Authentication flow
    void sendExchangeSeeds();
    void handleExchangeSeedsReply(const QByteArray &packet);
    void sendStartServer();
    void handleStartServerReply(const QByteArray &packet);

    // IFS attribute exchange
    void sendIFSExchangeAttributes();
    void handleIFSExchangeAttributesReply(const QByteArray &packet);

    // Response dispatch
    void dispatchPacket(const QByteArray &packet);
    void handleListAttrsReply(const QByteArray &packet);
    void handleOpenFileReply(const QByteArray &packet);
    void handleReadFileReply(const QByteArray &packet);
    void handleReturnCodeReply(const QByteArray &packet);

    // Packet builders
    QByteArray buildIFSPacket(uint16_t templateLength, uint16_t reqRepId,
                              const QByteArray &payload);
    void sendPacket(const QByteArray &packet);
    uint32_t nextCorrelation();

    // Helpers
    static QDateTime fromIBMTimestamp(uint64_t msEpoch);

    QTcpSocket *m_socket;
    QByteArray m_recvBuffer;
    State m_state;

    QString m_hostname;
    uint16_t m_port;
    QString m_userId;
    QString m_password;
    uint8_t m_passwordLevel;

    QByteArray m_clientSeed;
    QByteArray m_serverSeed;

    uint32_t m_correlationCounter;
    uint16_t m_datastreamLevel;
    uint32_t m_maxDataBlockSize;
    bool m_exchangeAttrsSent;

    // Accumulate list attrs entries
    QVector<IFSDirectoryEntry> m_listBuffer;
};

} // namespace hostserver
