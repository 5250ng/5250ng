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

#include "service_mapper.h"
#include "host_constants.h"
#include "host_data_stream.h"
#include "logger/logger.h"
#include <QEventLoop>
#include <QTimer>

namespace hostserver {

ServiceMapper::ServiceMapper(QObject *parent)
    : QObject(parent), m_socket(nullptr) {}

ServiceMapper::~ServiceMapper() {
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

void ServiceMapper::resolve(const QString &hostname, const QString &serviceName) {
    m_serviceName = serviceName;
    m_recvBuffer.clear();

    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
    }
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &ServiceMapper::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ServiceMapper::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ServiceMapper::onError);

    LOG_DEBUG(QString("[ServiceMapper]: Resolving '%1' on %2:%3")
                  .arg(serviceName, hostname)
                  .arg(ports::SERVICE_MAPPER));

    m_socket->connectToHost(hostname, ports::SERVICE_MAPPER);
}

uint16_t ServiceMapper::resolveSync(const QString &hostname,
                                    const QString &serviceName,
                                    int timeoutMs) {
    uint16_t result = 0;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(this, &ServiceMapper::portResolved, &loop,
            [&](const QString &, uint16_t port) {
                result = port;
                loop.quit();
            });
    connect(this, &ServiceMapper::errorOccurred, &loop, [&](const QString &) {
        loop.quit();
    });
    connect(&timer, &QTimer::timeout, &loop, [&]() {
        LOG_WARNING("[ServiceMapper]: Timeout resolving service");
        loop.quit();
    });

    timer.start(timeoutMs);
    resolve(hostname, serviceName);
    loop.exec();

    return result;
}

void ServiceMapper::onConnected() {
    // The service mapper protocol: send the service name as raw ASCII bytes
    QByteArray request = m_serviceName.toLatin1();
    m_socket->write(request);
    m_socket->flush();
}

void ServiceMapper::onReadyRead() {
    m_recvBuffer.append(m_socket->readAll());

    // Reply format: 1 byte status ('+' = 0x2B) + 4 bytes big-endian port
    if (m_recvBuffer.size() < 5) {
        return; // Wait for more data
    }
    QByteArray reply = m_recvBuffer;
    m_recvBuffer.clear();

    uint8_t status = static_cast<uint8_t>(reply[0]);
    if (status != 0x2B) { // '+' = success
        emit errorOccurred(QString("Service mapper returned error status 0x%1")
                               .arg(status, 2, 16, QLatin1Char('0')));
        m_socket->close();
        return;
    }

    uint16_t port = static_cast<uint16_t>(HostDataStream::readU32(reply, 1) & 0xFFFF);

    LOG_DEBUG(QString("[ServiceMapper]: '%1' resolved to port %2")
                  .arg(m_serviceName)
                  .arg(port));

    m_socket->close();
    emit portResolved(m_serviceName, port);
}

void ServiceMapper::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    QString msg = m_socket ? m_socket->errorString() : QStringLiteral("Unknown error");
    LOG_ERROR(QString("[ServiceMapper]: Socket error: %1").arg(msg));
    emit errorOccurred(msg);
}

} // namespace hostserver
