#include "worker.h"
#include "logger/logger.h"
#include "network/tn5250/message/message.h"
#include <QThread>
#include <sstream>
#include <vector>

namespace tn5250::session {

Worker::Worker(QObject *parent)
    : QObject(parent), m_client(nullptr) {}

Worker::~Worker() {
    stop();
}

void Worker::setConfig(const ::session::SessionConfig &cfg) {
    m_config = cfg;
}

void Worker::start() {
    if (m_client) {
        return;
    }
    m_client = new tn5250::client::TN5250Client(this);
    m_client->setDeviceName(m_config.deviceName());

    connect(m_client, &tn5250::client::TN5250Client::connected, this, &Worker::connected);
    connect(m_client, &tn5250::client::TN5250Client::disconnected, this, &Worker::disconnected);
    connect(m_client, &tn5250::client::TN5250Client::errorOccurred, this, &Worker::errorOccurred);
    connect(m_client, &tn5250::client::TN5250Client::stateChanged, this, &Worker::stateChanged);
    connect(m_client, &tn5250::client::TN5250Client::dataReceived, this, &Worker::onClientData, Qt::QueuedConnection);
    // Capture session logs from this thread only (DirectConnection executes in emitter's thread)
    connect(logger::Logger::instance(), &logger::Logger::logMessage, this, &Worker::onGlobalLogMessage, Qt::DirectConnection);

    logger::Logger::instance()->debug(
        QString("Session worker connecting to %1:%2 (TLS=%3)")
            .arg(m_config.hostname())
            .arg(m_config.port())
            .arg(m_config.useTLS())
    );
    m_client->connectToHost(m_config.hostname(), m_config.port(), m_config.useTLS());
}

void Worker::stop() {
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
        return;
    }
    m_client->sendData(data);
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

void Worker::onGlobalLogMessage(logger::LogLevel /*level*/, const QString &message) {
    // Only accept messages emitted from this worker's thread
    if (QThread::currentThread() != this->thread()) {
        return;
    }
    QMutexLocker locker(&m_logsMutex);
    m_logs.append(message);
    // Cap memory (keep last 5000 lines)
    const int maxLines = 5000;
    if (m_logs.size() > maxLines) {
        m_logs.erase(m_logs.begin(), m_logs.end() - maxLines);
    }
    emit sessionLogAppended(message);
}

} // namespace tn5250::session
