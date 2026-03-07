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
