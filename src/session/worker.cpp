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

#include "worker.h"
#include "logger/logger.h"
#include "tn5250/message/message.h"
#include <QThread>
#include <QTimer>
#include <sstream>
#include <vector>

namespace tn5250::session {

namespace {
// Replay pacing: original capture timing, clamped so the replay neither
// flashes past multi-record bursts nor stalls on operator think time.
constexpr int kReplayInitialDelayMs = 250;
constexpr qint64 kReplayMinStepMs = 40;
constexpr qint64 kReplayMaxStepMs = 1000;
} // namespace

Worker::Worker(QObject *parent)
    : QObject(parent), m_client(nullptr) {}

Worker::~Worker() {
    stop();
}

void Worker::setConfig(const ::session::SessionConfig &cfg) {
    m_config = cfg;
}

void Worker::start() {
    if (m_config.isReplay()) {
        startReplay();
        return;
    }
    if (m_client) {
        return;
    }
    m_client = new tn5250::client::TN5250Client(this);
    m_client->setDeviceName(m_config.deviceName());
    // Terminal type is the 5250 model identifier from the device type config
    QString termType = m_config.deviceType();
    if (termType.isEmpty()) {
        termType = (m_config.screenRows() > 24 || m_config.screenCols() > 80)
            ? QStringLiteral("IBM-3477-FC")
            : QStringLiteral("IBM-3179-2");
    }
    m_client->setTerminalType(termType);
    m_client->setCredentials(m_config.username(), m_config.password());
    m_client->setCodePage(m_config.codePage());
    m_client->setAllowInvalidCertificates(m_config.allowInvalidCertificates());

    connect(m_client, &tn5250::client::TN5250Client::connected, this, &Worker::connected);
    connect(m_client, &tn5250::client::TN5250Client::disconnected, this, &Worker::disconnected);
    connect(m_client, &tn5250::client::TN5250Client::errorOccurred, this, &Worker::errorOccurred);
    connect(m_client, &tn5250::client::TN5250Client::stateChanged, this, &Worker::stateChanged);
    connect(m_client, &tn5250::client::TN5250Client::dataReceived, this, &Worker::onClientData, Qt::QueuedConnection);

    logger::Logger::instance()->debug(
        QString("Session worker connecting to %1:%2 (TLS=%3)")
            .arg(m_config.hostname())
            .arg(m_config.port())
            .arg(m_config.useTLS())
    );
    m_client->connectToHost(m_config.hostname(), m_config.port(), m_config.useTLS());
}

void Worker::stop() {
    // Disconnect logger before destroying client to prevent use-after-free
    // from DirectConnection signals arriving from other threads
    disconnect(logger::Logger::instance(), nullptr, this, nullptr);
    if (m_replayTimer) {
        m_replayTimer->stop();
        m_replayTimer->deleteLater();
        m_replayTimer = nullptr;
        m_replayRecords.clear();
        m_replayIndex = 0;
        emit stateChanged(tn5250::client::TN5250Client::ConnectionState::Disconnected);
        emit disconnected();
    }
    if (!m_client) {
        return;
    }
    disconnect(m_client, nullptr, this, nullptr);
    m_client->disconnectFromHost();
    m_client->deleteLater();
    m_client = nullptr;
}

void Worker::sendInput(const QByteArray &data) {
    if (!m_client) {
        logger::Logger::instance()->warning("[Worker] sendInput: no client, dropping data");
        return;
    }
    logger::Logger::instance()->debug(
        QString("[Worker] sendInput: %1 bytes, hex=%2")
            .arg(data.size())
            .arg(QString::fromLatin1(data.left(64).toHex())));
    m_client->sendData(data);
}

void Worker::sendAttention() {
    if (!m_client) return;
    LOG_DEBUG("[Worker] sendAttention: GDS(flags=0x40, opcode=0, null)");
    m_client->sendGDS(0x40, 0x00, QByteArray());
}

void Worker::sendSystemRequest() {
    if (!m_client) return;
    LOG_DEBUG("[Worker] sendSystemRequest: GDS(flags=0x04, opcode=0, null)");
    m_client->sendGDS(0x04, 0x00, QByteArray());
}

void Worker::sendGDS(uint8_t flagsHi, uint8_t opcode, const QByteArray &payload) {
    if (!m_client) return;
    m_client->sendGDS(flagsHi, opcode, payload);
}

void Worker::onClientData(const QByteArray &data) {
    // Unmarshal and describe in debug mode; forward app data regardless
    std::vector<uint8_t> buf(reinterpret_cast<const uint8_t *>(data.constData()), reinterpret_cast<const uint8_t *>(data.constData()) + data.size());
    tn5250::message::Message msg;
    std::string err;
    if (msg.unmarshal(buf, &err)) {
        std::ostringstream os;
        msg.describe(os, 0);
        logger::Logger::instance()->debug_with_prefix("[TN5250->Message]", QString::fromStdString(os.str()));
    } else {
        logger::Logger::instance()->debug_with_prefix("[TN5250->Message]", QString("message unmarshal failed: %1").arg(QString::fromStdString(err)));
    }

    emit appData(data);
}

void Worker::startReplay() {
    if (m_replayTimer) {
        return;
    }
    core::pcap::ReplaySession replay;
    QString err;
    if (!core::pcap::PcapReplayLoader::loadFile(m_config.replayPcapFile(), &replay, &err)) {
        logger::Logger::instance()->error(
            QString("[Worker] PCAP replay failed: %1").arg(err));
        emit errorOccurred(err);
        emit stateChanged(tn5250::client::TN5250Client::ConnectionState::Disconnected);
        return;
    }
    logger::Logger::instance()->debug(
        QString("[Worker] Replaying %1 records from %2 (server %3, client %4)")
            .arg(replay.records.size())
            .arg(m_config.replayPcapFile())
            .arg(replay.serverEndpoint)
            .arg(replay.clientEndpoint));

    m_replayRecords = replay.records;
    m_replayIndex = 0;
    emit stateChanged(tn5250::client::TN5250Client::ConnectionState::Connected);
    emit connected();

    m_replayTimer = new QTimer(this);
    m_replayTimer->setSingleShot(true);
    connect(m_replayTimer, &QTimer::timeout, this, &Worker::onReplayTick);
    m_replayTimer->start(kReplayInitialDelayMs);
}

void Worker::onReplayTick() {
    if (m_replayIndex >= m_replayRecords.size()) {
        return;
    }
    const QByteArray data = m_replayRecords[m_replayIndex].data;
    const qint64 tsUsec = m_replayRecords[m_replayIndex].timestampUsec;
    m_replayIndex++;

    if (m_replayIndex < m_replayRecords.size()) {
        const qint64 deltaMs =
            (m_replayRecords[m_replayIndex].timestampUsec - tsUsec) / 1000;
        m_replayTimer->start(
            static_cast<int>(qBound(kReplayMinStepMs, deltaMs, kReplayMaxStepMs)));
    }

    onClientData(data);

    if (m_replayIndex >= m_replayRecords.size()) {
        logger::Logger::instance()->debug("[Worker] Replay finished");
        emit replayFinished();
    }
}

} // namespace tn5250::session
