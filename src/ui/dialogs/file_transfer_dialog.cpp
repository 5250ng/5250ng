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

#include "file_transfer_dialog.h"
#include "network/hostserver/host_constants.h"
#include "logger/logger.h"
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardPaths>

FileTransferDialog::FileTransferDialog(const session::SessionConfig &config,
                                       QWidget *parent)
    : ui::widgets::BaseFramelessDialog(parent)
    , m_config(config)
    , m_signonClient(nullptr)
    , m_ifsClient(nullptr)
    , m_transfer(nullptr)
    , m_currentRemotePath(QStringLiteral("/"))
    , m_currentLocalPath(QDir::homePath()) {
    setWindowTitle(QStringLiteral("File Transfer - %1").arg(config.hostname()));
    setupUI();
    connectToServer();
}

FileTransferDialog::~FileTransferDialog() {
    // m_signonClient, m_ifsClient, and m_transfer are parented to 'this',
    // so Qt will delete them automatically. No manual delete needed.
}

void FileTransferDialog::setupUI() {
    resize(900, 600);

    m_splitter = new QSplitter(Qt::Horizontal);

    // ---- Local pane (left) ----
    auto *localPane = new QWidget;
    auto *localLayout = new QVBoxLayout(localPane);
    localLayout->setContentsMargins(4, 4, 4, 4);
    localLayout->setSpacing(4);

    auto *localLabel = new QLabel(QStringLiteral("Local"));
    localLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));

    auto *localNavLayout = new QHBoxLayout;
    m_localUpButton = new QPushButton(QStringLiteral(".."));
    m_localUpButton->setFixedWidth(30);
    m_localUpButton->setToolTip(QStringLiteral("Go up one directory"));
    m_localPathEdit = new QLineEdit(m_currentLocalPath);
    localNavLayout->addWidget(m_localUpButton);
    localNavLayout->addWidget(m_localPathEdit);

    m_localModel = new QFileSystemModel(this);
    m_localModel->setRootPath(m_currentLocalPath);
    m_localModel->setFilter(QDir::AllEntries | QDir::NoDot);

    m_localTree = new QTreeView;
    m_localTree->setModel(m_localModel);
    m_localTree->setRootIndex(m_localModel->index(m_currentLocalPath));
    m_localTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_localTree->header()->setStretchLastSection(true);
    m_localTree->setColumnWidth(0, 200);
    // Hide extra columns (type, date modified) to keep it clean
    m_localTree->hideColumn(2);

    localLayout->addWidget(localLabel);
    localLayout->addLayout(localNavLayout);
    localLayout->addWidget(m_localTree);

    // ---- Transfer controls (center) ----
    auto *centerPane = new QWidget;
    auto *centerLayout = new QVBoxLayout(centerPane);
    centerLayout->setContentsMargins(4, 4, 4, 4);
    centerLayout->addStretch();

    m_downloadButton = new QPushButton(QStringLiteral("<< Download"));
    m_downloadButton->setToolTip(QStringLiteral("Download selected remote file"));
    m_downloadButton->setEnabled(false);

    m_uploadButton = new QPushButton(QStringLiteral("Upload >>"));
    m_uploadButton->setToolTip(QStringLiteral("Upload selected local file"));
    m_uploadButton->setEnabled(false);

    centerLayout->addWidget(m_downloadButton);
    centerLayout->addSpacing(8);
    centerLayout->addWidget(m_uploadButton);
    centerLayout->addStretch();

    // ---- Remote pane (right) ----
    auto *remotePane = new QWidget;
    auto *remoteLayout = new QVBoxLayout(remotePane);
    remoteLayout->setContentsMargins(4, 4, 4, 4);
    remoteLayout->setSpacing(4);

    auto *remoteLabel = new QLabel(QStringLiteral("Remote (IFS)"));
    remoteLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));

    auto *remoteNavLayout = new QHBoxLayout;
    m_remoteUpButton = new QPushButton(QStringLiteral(".."));
    m_remoteUpButton->setFixedWidth(30);
    m_remoteUpButton->setToolTip(QStringLiteral("Go up one directory"));
    m_remotePathEdit = new QLineEdit(m_currentRemotePath);
    m_remoteRefreshButton = new QPushButton(QStringLiteral("Refresh"));
    m_remoteRefreshButton->setFixedWidth(60);
    remoteNavLayout->addWidget(m_remoteUpButton);
    remoteNavLayout->addWidget(m_remotePathEdit);
    remoteNavLayout->addWidget(m_remoteRefreshButton);

    m_remoteModel = new QStandardItemModel(0, 4, this);
    m_remoteModel->setHorizontalHeaderLabels(
        {QStringLiteral("Name"), QStringLiteral("Size"),
         QStringLiteral("Type"), QStringLiteral("Modified")});

    m_remoteTree = new QTreeView;
    m_remoteTree->setModel(m_remoteModel);
    m_remoteTree->setRootIsDecorated(false);
    m_remoteTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_remoteTree->header()->setStretchLastSection(true);
    m_remoteTree->setColumnWidth(0, 200);
    m_remoteTree->setColumnWidth(1, 80);
    m_remoteTree->setColumnWidth(2, 60);

    auto *remoteOpsLayout = new QHBoxLayout;
    m_remoteMkdirButton = new QPushButton(QStringLiteral("New Folder"));
    m_remoteDeleteButton = new QPushButton(QStringLiteral("Delete"));
    m_remoteRenameButton = new QPushButton(QStringLiteral("Rename"));
    m_remoteMkdirButton->setEnabled(false);
    m_remoteDeleteButton->setEnabled(false);
    m_remoteRenameButton->setEnabled(false);
    remoteOpsLayout->addWidget(m_remoteMkdirButton);
    remoteOpsLayout->addWidget(m_remoteDeleteButton);
    remoteOpsLayout->addWidget(m_remoteRenameButton);
    remoteOpsLayout->addStretch();

    remoteLayout->addWidget(remoteLabel);
    remoteLayout->addLayout(remoteNavLayout);
    remoteLayout->addWidget(m_remoteTree);
    remoteLayout->addLayout(remoteOpsLayout);

    // ---- Assemble splitter ----
    m_splitter->addWidget(localPane);
    m_splitter->addWidget(centerPane);
    m_splitter->addWidget(remotePane);
    m_splitter->setStretchFactor(0, 2);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 2);

    // ---- Progress bar and status ----
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(true);

    m_statusLabel = new QLabel(QStringLiteral("Connecting..."));

    // ---- Final layout ----
    contentLayout()->addWidget(m_splitter);
    contentLayout()->addWidget(m_progressBar);
    contentLayout()->addWidget(m_statusLabel);

    // ---- Signals ----
    connect(m_localTree, &QTreeView::doubleClicked, this, &FileTransferDialog::onLocalItemDoubleClicked);
    connect(m_remoteTree, &QTreeView::doubleClicked, this, &FileTransferDialog::onRemoteItemDoubleClicked);
    connect(m_localPathEdit, &QLineEdit::returnPressed, this, &FileTransferDialog::onLocalPathEntered);
    connect(m_remotePathEdit, &QLineEdit::returnPressed, this, &FileTransferDialog::onRemotePathEntered);
    connect(m_localUpButton, &QPushButton::clicked, this, &FileTransferDialog::onLocalGoUp);
    connect(m_remoteUpButton, &QPushButton::clicked, this, &FileTransferDialog::onRemoteGoUp);
    connect(m_remoteRefreshButton, &QPushButton::clicked, this, &FileTransferDialog::onRefreshRemote);
    connect(m_downloadButton, &QPushButton::clicked, this, &FileTransferDialog::onDownloadClicked);
    connect(m_uploadButton, &QPushButton::clicked, this, &FileTransferDialog::onUploadClicked);
    connect(m_remoteMkdirButton, &QPushButton::clicked, this, &FileTransferDialog::onRemoteCreateDir);
    connect(m_remoteDeleteButton, &QPushButton::clicked, this, &FileTransferDialog::onRemoteDelete);
    connect(m_remoteRenameButton, &QPushButton::clicked, this, &FileTransferDialog::onRemoteRename);

    setRemoteEnabled(false);
}

void FileTransferDialog::setRemoteEnabled(bool enabled) {
    m_downloadButton->setEnabled(enabled);
    m_uploadButton->setEnabled(enabled);
    m_remoteMkdirButton->setEnabled(enabled);
    m_remoteDeleteButton->setEnabled(enabled);
    m_remoteRenameButton->setEnabled(enabled);
    m_remoteTree->setEnabled(enabled);
    m_remotePathEdit->setEnabled(enabled);
    m_remoteUpButton->setEnabled(enabled);
    m_remoteRefreshButton->setEnabled(enabled);
}

void FileTransferDialog::setStatus(const QString &msg) {
    m_statusLabel->setText(msg);
}

void FileTransferDialog::connectToServer() {
    setStatus(QStringLiteral("Authenticating with %1...").arg(m_config.hostname()));

    m_signonClient = new hostserver::SignonClient(this);
    connect(m_signonClient, &hostserver::SignonClient::authenticated,
            this, &FileTransferDialog::onSignonAuthenticated);
    connect(m_signonClient, &hostserver::SignonClient::errorOccurred,
            this, &FileTransferDialog::onSignonError);

    m_signonClient->signon(m_config.hostname(), m_config.username(), m_config.password());
}

void FileTransferDialog::onSignonAuthenticated() {
    setStatus(QStringLiteral("Signed on. Connecting to IFS file server..."));

    m_ifsClient = new hostserver::IFSClient(this);
    connect(m_ifsClient, &hostserver::IFSClient::connected,
            this, &FileTransferDialog::onIFSConnected);
    connect(m_ifsClient, &hostserver::IFSClient::errorOccurred,
            this, &FileTransferDialog::onIFSError);
    connect(m_ifsClient, &hostserver::IFSClient::directoryListed,
            this, &FileTransferDialog::onDirectoryListed);
    connect(m_ifsClient, &hostserver::IFSClient::operationCompleted,
            this, [this](uint16_t, uint16_t rc) {
                if (rc == hostserver::ifs_rc::SUCCESS) {
                    onRefreshRemote(); // Refresh after successful operation
                } else {
                    setStatus(QString("Operation failed: %1")
                                  .arg(hostserver::ifs_rc::toString(rc)));
                }
            });

    m_ifsClient->connectToHost(
        m_config.hostname(),
        m_config.username(),
        m_config.password(),
        m_signonClient->passwordLevel());
}

void FileTransferDialog::onSignonError(const QString &error) {
    setStatus(QStringLiteral("Signon failed: %1").arg(error));
}

void FileTransferDialog::onIFSConnected() {
    setStatus(QStringLiteral("Connected to IFS file server"));
    setRemoteEnabled(true);
    navigateRemote(m_currentRemotePath);
}

void FileTransferDialog::onIFSError(const QString &error) {
    setStatus(QStringLiteral("IFS error: %1").arg(error));
}

void FileTransferDialog::navigateRemote(const QString &path) {
    m_currentRemotePath = path;
    m_remotePathEdit->setText(path);

    // List with wildcard
    QString pattern = path;
    if (!pattern.endsWith(QLatin1Char('/'))) pattern.append(QLatin1Char('/'));
    pattern.append(QLatin1Char('*'));

    setStatus(QStringLiteral("Listing %1 ...").arg(path));
    m_ifsClient->listDirectory(pattern);
}

void FileTransferDialog::navigateLocal(const QString &path) {
    m_currentLocalPath = path;
    m_localPathEdit->setText(path);
    m_localTree->setRootIndex(m_localModel->index(path));
    m_localModel->setRootPath(path);
}

void FileTransferDialog::onDirectoryListed(const QVector<hostserver::IFSDirectoryEntry> &entries) {
    m_remoteModel->removeRows(0, m_remoteModel->rowCount());

    for (const auto &entry : entries) {
        if (entry.name == QLatin1String(".") || entry.name == QLatin1String(".."))
            continue;

        QList<QStandardItem *> row;

        auto *nameItem = new QStandardItem(entry.name);
        nameItem->setEditable(false);
        if (entry.isDirectory()) {
            nameItem->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            nameItem->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        }
        // Store object type in user data
        nameItem->setData(entry.isDirectory(), Qt::UserRole + 1);
        nameItem->setData(entry.name, Qt::UserRole + 2);

        auto *sizeItem = new QStandardItem;
        sizeItem->setEditable(false);
        if (!entry.isDirectory()) {
            if (entry.fileSize >= 1024 * 1024) {
                sizeItem->setText(QString("%1 MB").arg(entry.fileSize / (1024 * 1024)));
            } else if (entry.fileSize >= 1024) {
                sizeItem->setText(QString("%1 KB").arg(entry.fileSize / 1024));
            } else {
                sizeItem->setText(QString("%1 B").arg(entry.fileSize));
            }
        }
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto *typeItem = new QStandardItem(entry.isDirectory() ? QStringLiteral("Dir") : QStringLiteral("File"));
        typeItem->setEditable(false);

        auto *dateItem = new QStandardItem(entry.modifyDate.toString(Qt::ISODate));
        dateItem->setEditable(false);

        row << nameItem << sizeItem << typeItem << dateItem;
        m_remoteModel->appendRow(row);
    }

    setStatus(QStringLiteral("%1 - %2 items").arg(m_currentRemotePath).arg(entries.size()));
}

void FileTransferDialog::onRemoteItemDoubleClicked(const QModelIndex &index) {
    if (!index.isValid()) return;

    QModelIndex nameIndex = m_remoteModel->index(index.row(), 0);
    bool isDir = nameIndex.data(Qt::UserRole + 1).toBool();
    QString name = nameIndex.data(Qt::UserRole + 2).toString();

    if (isDir) {
        QString newPath = m_currentRemotePath;
        if (!newPath.endsWith(QLatin1Char('/'))) newPath.append(QLatin1Char('/'));
        newPath.append(name);
        navigateRemote(newPath);
    }
}

void FileTransferDialog::onLocalItemDoubleClicked(const QModelIndex &index) {
    if (!index.isValid()) return;

    QFileInfo info = m_localModel->fileInfo(index);
    if (info.isDir()) {
        navigateLocal(info.absoluteFilePath());
    }
}

void FileTransferDialog::onRemotePathEntered() {
    navigateRemote(m_remotePathEdit->text());
}

void FileTransferDialog::onLocalPathEntered() {
    navigateLocal(m_localPathEdit->text());
}

void FileTransferDialog::onRemoteGoUp() {
    QString path = m_currentRemotePath;
    if (path == QLatin1String("/")) return;
    int lastSlash = path.lastIndexOf(QLatin1Char('/'));
    if (lastSlash <= 0) {
        navigateRemote(QStringLiteral("/"));
    } else {
        navigateRemote(path.left(lastSlash));
    }
}

void FileTransferDialog::onLocalGoUp() {
    QDir dir(m_currentLocalPath);
    if (dir.cdUp()) {
        navigateLocal(dir.absolutePath());
    }
}

void FileTransferDialog::onRefreshRemote() {
    navigateRemote(m_currentRemotePath);
}

void FileTransferDialog::onDownloadClicked() {
    QModelIndex index = m_remoteTree->currentIndex();
    if (!index.isValid()) return;

    QModelIndex nameIndex = m_remoteModel->index(index.row(), 0);
    bool isDir = nameIndex.data(Qt::UserRole + 1).toBool();
    QString name = nameIndex.data(Qt::UserRole + 2).toString();

    if (isDir) {
        setStatus(QStringLiteral("Cannot download directories"));
        return;
    }

    QString remotePath = m_currentRemotePath;
    if (!remotePath.endsWith(QLatin1Char('/'))) remotePath.append(QLatin1Char('/'));
    remotePath.append(name);

    QString localPath = m_currentLocalPath;
    if (!localPath.endsWith(QDir::separator())) localPath.append(QDir::separator());
    localPath.append(name);

    setStatus(QStringLiteral("Downloading %1 ...").arg(name));
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    delete m_transfer;
    m_transfer = new hostserver::IFSTransfer(this);
    connect(m_transfer, &hostserver::IFSTransfer::progressChanged,
            this, &FileTransferDialog::onTransferProgress);
    connect(m_transfer, &hostserver::IFSTransfer::transferComplete,
            this, &FileTransferDialog::onTransferComplete);
    connect(m_transfer, &hostserver::IFSTransfer::errorOccurred,
            this, &FileTransferDialog::onTransferError);

    m_transfer->download(m_config.hostname(), hostserver::ports::FILE,
                          m_config.username(), m_config.password(),
                          m_signonClient->passwordLevel(),
                          remotePath, localPath);
}

void FileTransferDialog::onUploadClicked() {
    QModelIndex index = m_localTree->currentIndex();
    if (!index.isValid()) return;

    QFileInfo info = m_localModel->fileInfo(index);
    if (info.isDir()) {
        setStatus(QStringLiteral("Cannot upload directories"));
        return;
    }

    QString localPath = info.absoluteFilePath();
    QString remotePath = m_currentRemotePath;
    if (!remotePath.endsWith(QLatin1Char('/'))) remotePath.append(QLatin1Char('/'));
    remotePath.append(info.fileName());

    setStatus(QStringLiteral("Uploading %1 ...").arg(info.fileName()));
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    delete m_transfer;
    m_transfer = new hostserver::IFSTransfer(this);
    connect(m_transfer, &hostserver::IFSTransfer::progressChanged,
            this, &FileTransferDialog::onTransferProgress);
    connect(m_transfer, &hostserver::IFSTransfer::transferComplete,
            this, &FileTransferDialog::onTransferComplete);
    connect(m_transfer, &hostserver::IFSTransfer::errorOccurred,
            this, &FileTransferDialog::onTransferError);

    m_transfer->upload(m_config.hostname(), hostserver::ports::FILE,
                        m_config.username(), m_config.password(),
                        m_signonClient->passwordLevel(),
                        localPath, remotePath);
}

void FileTransferDialog::onTransferProgress(uint64_t transferred, uint64_t total) {
    if (total > 0) {
        int pct = static_cast<int>((transferred * 100) / total);
        m_progressBar->setValue(pct);

        QString sizeStr;
        if (total >= 1024 * 1024) {
            sizeStr = QString("%1 / %2 MB")
                          .arg(transferred / (1024 * 1024))
                          .arg(total / (1024 * 1024));
        } else {
            sizeStr = QString("%1 / %2 KB")
                          .arg(transferred / 1024)
                          .arg(total / 1024);
        }
        setStatus(sizeStr);
    }
}

void FileTransferDialog::onTransferComplete() {
    m_progressBar->setValue(100);
    setStatus(QStringLiteral("Transfer complete"));

    // Refresh both panes
    onRefreshRemote();
    navigateLocal(m_currentLocalPath);
}

void FileTransferDialog::onTransferError(const QString &error) {
    m_progressBar->setVisible(false);
    setStatus(QStringLiteral("Transfer failed: %1").arg(error));
}

void FileTransferDialog::onRemoteCreateDir() {
    bool ok;
    QString name = QInputDialog::getText(this, QStringLiteral("New Folder"),
                                          QStringLiteral("Folder name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) return;

    QString fullPath = m_currentRemotePath;
    if (!fullPath.endsWith(QLatin1Char('/'))) fullPath.append(QLatin1Char('/'));
    fullPath.append(name);

    m_ifsClient->createDirectory(fullPath);
    setStatus(QStringLiteral("Creating folder: %1").arg(name));
}

void FileTransferDialog::onRemoteDelete() {
    QModelIndex index = m_remoteTree->currentIndex();
    if (!index.isValid()) return;

    QModelIndex nameIndex = m_remoteModel->index(index.row(), 0);
    bool isDir = nameIndex.data(Qt::UserRole + 1).toBool();
    QString name = nameIndex.data(Qt::UserRole + 2).toString();

    auto answer = QMessageBox::question(this, QStringLiteral("Confirm Delete"),
                                         QStringLiteral("Delete '%1'?").arg(name));
    if (answer != QMessageBox::Yes) return;

    QString fullPath = m_currentRemotePath;
    if (!fullPath.endsWith(QLatin1Char('/'))) fullPath.append(QLatin1Char('/'));
    fullPath.append(name);

    if (isDir) {
        m_ifsClient->deleteDirectory(fullPath);
    } else {
        m_ifsClient->deleteFile(fullPath);
    }
    setStatus(QStringLiteral("Deleting: %1").arg(name));
}

void FileTransferDialog::onRemoteRename() {
    QModelIndex index = m_remoteTree->currentIndex();
    if (!index.isValid()) return;

    QModelIndex nameIndex = m_remoteModel->index(index.row(), 0);
    QString name = nameIndex.data(Qt::UserRole + 2).toString();

    bool ok;
    QString newName = QInputDialog::getText(this, QStringLiteral("Rename"),
                                             QStringLiteral("New name:"),
                                             QLineEdit::Normal, name, &ok);
    if (!ok || newName.isEmpty() || newName == name) return;

    QString base = m_currentRemotePath;
    if (!base.endsWith(QLatin1Char('/'))) base.append(QLatin1Char('/'));

    m_ifsClient->rename(base + name, base + newName);
    setStatus(QStringLiteral("Renaming: %1 -> %2").arg(name, newName));
}
