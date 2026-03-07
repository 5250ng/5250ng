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

#include "ifs_client.h"
#include <QFile>
#include <QObject>
#include <QString>
#include <cstdint>

namespace hostserver {

// High-level file transfer coordinator that uses IFSClient for
// downloading and uploading files between the local system and IBM i IFS.
//
// Handles chunked reads/writes, progress reporting, and error recovery.
class IFSTransfer : public QObject {
    Q_OBJECT

  public:
    enum class Direction { Download, Upload };
    enum class TransferState { Idle, Connecting, Transferring, Complete, Error };

    explicit IFSTransfer(QObject *parent = nullptr);
    ~IFSTransfer();

    // Download a remote IFS file to a local path
    void download(const QString &hostname, uint16_t port,
                  const QString &userId, const QString &password,
                  uint8_t passwordLevel,
                  const QString &remotePath, const QString &localPath);

    // Upload a local file to a remote IFS path
    void upload(const QString &hostname, uint16_t port,
                const QString &userId, const QString &password,
                uint8_t passwordLevel,
                const QString &localPath, const QString &remotePath);

    // Cancel an in-progress transfer
    void cancel();

    TransferState state() const { return m_state; }
    Direction direction() const { return m_direction; }
    uint64_t bytesTransferred() const { return m_bytesTransferred; }
    uint64_t totalBytes() const { return m_totalBytes; }
    double progress() const;

  signals:
    void progressChanged(uint64_t bytesTransferred, uint64_t totalBytes);
    void transferComplete();
    void errorOccurred(const QString &error);
    void stateChanged(TransferState state);

  private slots:
    void onIFSConnected();
    void onIFSError(const QString &error);
    void onFileOpened(const IFSFileHandle &handle);
    void onFileDataRead(uint32_t fileHandle, const QByteArray &data);
    void onOperationCompleted(uint16_t requestId, uint16_t returnCode);

  private:
    void setState(TransferState newState);
    void startDownload();
    void readNextChunk();
    void startUpload();
    void writeNextChunk();

    IFSClient *m_client;
    QFile *m_localFile;
    TransferState m_state;
    Direction m_direction;
    bool m_cancelled;

    QString m_remotePath;
    QString m_localPath;

    uint32_t m_fileHandle;
    uint64_t m_bytesTransferred;
    uint64_t m_totalBytes;
    uint64_t m_currentOffset;

    static constexpr uint32_t CHUNK_SIZE = 512 * 1024; // 512 KB chunks
};

} // namespace hostserver
