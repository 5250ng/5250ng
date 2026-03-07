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

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtNetwork/QTcpSocket>
#include <cstdint>

namespace hostserver {

// Resolves IBM i host server service names to port numbers via
// the service mapper on TCP port 449.
//
// Usage:
//   ServiceMapper mapper;
//   connect(&mapper, &ServiceMapper::portResolved, ...);
//   connect(&mapper, &ServiceMapper::errorOccurred, ...);
//   mapper.resolve("myhost.example.com", "as-file");
class ServiceMapper : public QObject {
    Q_OBJECT

  public:
    explicit ServiceMapper(QObject *parent = nullptr);
    ~ServiceMapper();

    // Resolve a service name (e.g. "as-file") via the mapper on the given host.
    // Emits portResolved on success or errorOccurred on failure.
    void resolve(const QString &hostname, const QString &serviceName);

    // Synchronous resolution (blocks up to timeoutMs). Returns 0 on failure.
    uint16_t resolveSync(const QString &hostname, const QString &serviceName,
                         int timeoutMs = 5000);

    // Well-known service names
    static constexpr const char *SERVICE_SIGNON   = "as-signon";
    static constexpr const char *SERVICE_FILE     = "as-file";
    static constexpr const char *SERVICE_DATABASE = "as-database";
    static constexpr const char *SERVICE_CENTRAL  = "as-central";
    static constexpr const char *SERVICE_COMMAND  = "as-rmtcmd";
    static constexpr const char *SERVICE_PRINT    = "as-netprt";
    static constexpr const char *SERVICE_DTAQ     = "as-dtaq";

  signals:
    void portResolved(const QString &serviceName, uint16_t port);
    void errorOccurred(const QString &error);

  private slots:
    void onConnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);

  private:
    QTcpSocket *m_socket;
    QByteArray m_recvBuffer;
    QString m_serviceName;
};

} // namespace hostserver
