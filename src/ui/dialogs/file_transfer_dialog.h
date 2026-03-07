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

#include "network/hostserver/ifs_client.h"
#include "network/hostserver/ifs_transfer.h"
#include "network/hostserver/signon_client.h"
#include "session/config.h"
#include "ui/widgets/Frameless/BaseFramelessDialog.h"
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

class FileTransferDialog : public ui::widgets::BaseFramelessDialog {
    Q_OBJECT

  public:
    explicit FileTransferDialog(const session::SessionConfig &config,
                                QWidget *parent = nullptr);
    ~FileTransferDialog();

  private slots:
    // Signon flow
    void onSignonAuthenticated();
    void onSignonError(const QString &error);

    // IFS connection
    void onIFSConnected();
    void onIFSError(const QString &error);
    void onDirectoryListed(const QVector<hostserver::IFSDirectoryEntry> &entries);

    // Navigation
    void onRemoteItemDoubleClicked(const QModelIndex &index);
    void onLocalItemDoubleClicked(const QModelIndex &index);
    void onRemotePathEntered();
    void onLocalPathEntered();
    void onRemoteGoUp();
    void onLocalGoUp();

    // Transfer
    void onDownloadClicked();
    void onUploadClicked();
    void onTransferProgress(uint64_t transferred, uint64_t total);
    void onTransferComplete();
    void onTransferError(const QString &error);

    // File operations
    void onRemoteCreateDir();
    void onRemoteDelete();
    void onRemoteRename();
    void onRefreshRemote();

  private:
    void setupUI();
    void connectToServer();
    void navigateRemote(const QString &path);
    void navigateLocal(const QString &path);
    void setStatus(const QString &msg);
    void setRemoteEnabled(bool enabled);

    // Widgets
    QSplitter *m_splitter;

    // Local pane
    QTreeView *m_localTree;
    QFileSystemModel *m_localModel;
    QLineEdit *m_localPathEdit;
    QPushButton *m_localUpButton;

    // Remote pane
    QTreeView *m_remoteTree;
    QStandardItemModel *m_remoteModel;
    QLineEdit *m_remotePathEdit;
    QPushButton *m_remoteUpButton;
    QPushButton *m_remoteRefreshButton;

    // Transfer controls
    QPushButton *m_downloadButton;
    QPushButton *m_uploadButton;
    QPushButton *m_remoteMkdirButton;
    QPushButton *m_remoteDeleteButton;
    QPushButton *m_remoteRenameButton;

    // Progress
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;

    // State
    session::SessionConfig m_config;
    hostserver::SignonClient *m_signonClient;
    hostserver::IFSClient *m_ifsClient;
    hostserver::IFSTransfer *m_transfer;

    QString m_currentRemotePath;
    QString m_currentLocalPath;
};
