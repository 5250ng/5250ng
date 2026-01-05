#include "client.h"
#include "logger/logger.h"

#ifdef HAVE_QT6_SSL
#include <QSslSocket>
#endif

namespace tn5250::client {

using telnet::TelnetCommand;
using telnet::TelnetOption;

void TN5250Client::startTLS() {
#ifdef HAVE_QT6_SSL
    if (!m_sslSocket || m_tlsStarted) {
        return;
    }
    sendTelnetCommand(TelnetCommand::DO, TelnetOption::TELNET_START_TLS);
#else
    logger::Logger::instance()->warning("[TN5250->Client]: TLS not supported");
#endif
}

void TN5250Client::handleStartTLS() {
#ifdef HAVE_QT6_SSL
    if (m_sslSocket && !m_tlsStarted) {
        logger::Logger::instance()->debug("[TN5250->Client]: Starting TLS negotiation");
        static_cast<QSslSocket *>(m_sslSocket)->startClientEncryption();
    }
#else
    logger::Logger::instance()->warning("[TN5250->Client]: TLS not supported");
#endif
}

} // namespace tn5250::client
