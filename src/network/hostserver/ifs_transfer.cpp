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

#include "ifs_transfer.h"
#include "host_constants.h"
#include "logger/logger.h"

namespace hostserver {

IFSTransfer::IFSTransfer(QObject *parent)
    : QObject(parent)
    , m_client(nullptr)
    , m_localFile(nullptr)
    , m_state(TransferState::Idle)
    , m_direction(Direction::Download)
    , m_cancelled(false)
    , m_fileHandle(0)
    , m_bytesTransferred(0)
    , m_totalBytes(0)
    , m_currentOffset(0) {}

IFSTransfer::~IFSTransfer() {
    // Cancel without sending closeFile - socket may already be invalid during destruction
    m_cancelled = true;
    if (m_localFile) {
        m_localFile->close();
        delete m_localFile;
        m_localFile = nullptr;
    }
    // m_client is parented to 'this', Qt will delete it automatically
}

void IFSTransfer::setState(TransferState newState) {
    if (m_state == newState) return;
    m_state = newState;
    emit stateChanged(m_state);
}

double IFSTransfer::progress() const {
    if (m_totalBytes == 0) return 0.0;
    return static_cast<double>(m_bytesTransferred) / static_cast<double>(m_totalBytes);
}

void IFSTransfer::cancel() {
    m_cancelled = true;
    if (m_client && m_client->isReady() && m_fileHandle != 0) {
        m_client->closeFile(m_fileHandle);
        m_fileHandle = 0;
    }
    if (m_localFile) {
        m_localFile->close();
    }
    if (m_state == TransferState::Transferring || m_state == TransferState::Connecting) {
        setState(TransferState::Error);
        emit errorOccurred("Transfer cancelled");
    }
}

void IFSTransfer::download(const QString &hostname, uint16_t port,
                            const QString &userId, const QString &password,
                            uint8_t passwordLevel,
                            const QString &remotePath, const QString &localPath) {
    m_direction = Direction::Download;
    m_remotePath = remotePath;
    m_localPath = localPath;
    m_bytesTransferred = 0;
    m_totalBytes = 0;
    m_currentOffset = 0;
    m_fileHandle = 0;
    m_cancelled = false;

    // Prepare local file for writing
    delete m_localFile;
    m_localFile = new QFile(localPath);
    if (!m_localFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setState(TransferState::Error);
        emit errorOccurred(QString("Cannot open local file: %1").arg(m_localFile->errorString()));
        return;
    }

    // Connect to IFS server
    delete m_client;
    m_client = new IFSClient(this);
    connect(m_client, &IFSClient::connected, this, &IFSTransfer::onIFSConnected);
    connect(m_client, &IFSClient::errorOccurred, this, &IFSTransfer::onIFSError);
    connect(m_client, &IFSClient::fileOpened, this, &IFSTransfer::onFileOpened);
    connect(m_client, &IFSClient::fileDataRead, this, &IFSTransfer::onFileDataRead);
    connect(m_client, &IFSClient::operationCompleted, this, &IFSTransfer::onOperationCompleted);

    setState(TransferState::Connecting);
    m_client->connectToHost(hostname, port, userId, password, passwordLevel);
}

void IFSTransfer::upload(const QString &hostname, uint16_t port,
                          const QString &userId, const QString &password,
                          uint8_t passwordLevel,
                          const QString &localPath, const QString &remotePath) {
    m_direction = Direction::Upload;
    m_remotePath = remotePath;
    m_localPath = localPath;
    m_bytesTransferred = 0;
    m_currentOffset = 0;
    m_fileHandle = 0;
    m_cancelled = false;

    // Open local file for reading
    delete m_localFile;
    m_localFile = new QFile(localPath);
    if (!m_localFile->open(QIODevice::ReadOnly)) {
        setState(TransferState::Error);
        emit errorOccurred(QString("Cannot open local file: %1").arg(m_localFile->errorString()));
        return;
    }
    m_totalBytes = static_cast<uint64_t>(m_localFile->size());

    // Connect to IFS server
    delete m_client;
    m_client = new IFSClient(this);
    connect(m_client, &IFSClient::connected, this, &IFSTransfer::onIFSConnected);
    connect(m_client, &IFSClient::errorOccurred, this, &IFSTransfer::onIFSError);
    connect(m_client, &IFSClient::fileOpened, this, &IFSTransfer::onFileOpened);
    connect(m_client, &IFSClient::operationCompleted, this, &IFSTransfer::onOperationCompleted);

    setState(TransferState::Connecting);
    m_client->connectToHost(hostname, port, userId, password, passwordLevel);
}

void IFSTransfer::onIFSConnected() {
    LOG_DEBUG(QString("[IFSTransfer]: Connected, opening remote file: %1").arg(m_remotePath));
    if (m_direction == Direction::Download) {
        startDownload();
    } else {
        startUpload();
    }
}

void IFSTransfer::onIFSError(const QString &error) {
    if (m_localFile) m_localFile->close();
    setState(TransferState::Error);
    emit errorOccurred(error);
}

void IFSTransfer::startDownload() {
    m_client->openFileForRead(m_remotePath);
}

void IFSTransfer::startUpload() {
    m_client->openFileForWrite(m_remotePath);
}

void IFSTransfer::onFileOpened(const IFSFileHandle &handle) {
    if (!handle.valid) {
        setState(TransferState::Error);
        emit errorOccurred("Failed to open remote file");
        return;
    }

    m_fileHandle = handle.handle;
    setState(TransferState::Transferring);

    if (m_direction == Direction::Download) {
        m_totalBytes = handle.fileSize;
        LOG_DEBUG(QString("[IFSTransfer]: Download started, file size=%1")
                      .arg(m_totalBytes));
        readNextChunk();
    } else {
        LOG_DEBUG(QString("[IFSTransfer]: Upload started, local size=%1")
                      .arg(m_totalBytes));
        writeNextChunk();
    }
}

void IFSTransfer::readNextChunk() {
    if (m_cancelled) return;

    uint32_t chunkSize = CHUNK_SIZE;
    // Don't exceed max data block size if the server specified one
    if (m_client->maxDataBlockSize() > 0 && chunkSize > m_client->maxDataBlockSize()) {
        chunkSize = m_client->maxDataBlockSize();
    }

    m_client->readFile(m_fileHandle, m_currentOffset, chunkSize);
}

void IFSTransfer::onFileDataRead(uint32_t fileHandle, const QByteArray &data) {
    Q_UNUSED(fileHandle)

    if (m_cancelled) return;

    if (data.isEmpty()) {
        // EOF reached
        m_client->closeFile(m_fileHandle);
        m_fileHandle = 0;
        if (m_localFile) m_localFile->close();
        setState(TransferState::Complete);
        LOG_INFO(QString("[IFSTransfer]: Download complete, %1 bytes transferred")
                     .arg(m_bytesTransferred));
        emit transferComplete();
        return;
    }

    // Write chunk to local file
    qint64 written = m_localFile->write(data);
    if (written < 0) {
        setState(TransferState::Error);
        emit errorOccurred(QString("Local write error: %1").arg(m_localFile->errorString()));
        return;
    }

    m_bytesTransferred += static_cast<uint64_t>(written);
    m_currentOffset += static_cast<uint64_t>(written);
    emit progressChanged(m_bytesTransferred, m_totalBytes);

    // Read next chunk
    readNextChunk();
}

void IFSTransfer::writeNextChunk() {
    if (m_cancelled) return;

    uint32_t chunkSize = CHUNK_SIZE;
    if (m_client->maxDataBlockSize() > 0 && chunkSize > m_client->maxDataBlockSize() - 256) {
        chunkSize = m_client->maxDataBlockSize() - 256; // Leave room for packet overhead
    }

    QByteArray data = m_localFile->read(static_cast<qint64>(chunkSize));
    if (data.isEmpty()) {
        // EOF - all data written
        m_client->closeFile(m_fileHandle);
        m_fileHandle = 0;
        if (m_localFile) m_localFile->close();
        setState(TransferState::Complete);
        LOG_INFO(QString("[IFSTransfer]: Upload complete, %1 bytes transferred")
                     .arg(m_bytesTransferred));
        emit transferComplete();
        return;
    }

    m_client->writeFile(m_fileHandle, m_currentOffset, data);
    m_bytesTransferred += static_cast<uint64_t>(data.size());
    m_currentOffset += static_cast<uint64_t>(data.size());
    emit progressChanged(m_bytesTransferred, m_totalBytes);
}

void IFSTransfer::onOperationCompleted(uint16_t requestId, uint16_t returnCode) {
    Q_UNUSED(requestId)

    if (returnCode != ifs_rc::SUCCESS && returnCode != ifs_rc::NO_MORE_DATA) {
        if (m_state == TransferState::Transferring) {
            setState(TransferState::Error);
            emit errorOccurred(QString("Transfer error: %1").arg(ifs_rc::toString(returnCode)));
        }
        return;
    }

    // For upload: the return code reply acknowledges a write, continue with next chunk
    if (m_direction == Direction::Upload && m_state == TransferState::Transferring) {
        writeNextChunk();
    }
}

} // namespace hostserver
