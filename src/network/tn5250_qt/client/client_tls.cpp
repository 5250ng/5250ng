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

#include <QFile>

#ifdef HAVE_QT6_SSL
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
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

#ifdef HAVE_QT6_SSL
namespace {

QList<QSslCertificate> loadCertificates(const QString &path) {
    QList<QSslCertificate> certs = QSslCertificate::fromPath(path, QSsl::Pem);
    if (certs.isEmpty())
        certs = QSslCertificate::fromPath(path, QSsl::Der);
    return certs;
}

QSslKey loadPrivateKey(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QSslKey();
    const QByteArray data = file.readAll();
    for (QSsl::EncodingFormat format : {QSsl::Pem, QSsl::Der}) {
        for (QSsl::KeyAlgorithm algorithm : {QSsl::Rsa, QSsl::Ec, QSsl::Dsa}) {
            QSslKey key(data, algorithm, format, QSsl::PrivateKey);
            if (!key.isNull())
                return key;
        }
    }
    return QSslKey();
}

} // namespace
#endif

bool TN5250Client::applyTlsFiles(QString *error) {
#ifdef HAVE_QT6_SSL
    if (!m_sslSocket)
        return true;
    if (m_tlsCaCertificateFile.isEmpty() && m_tlsClientCertificateFile.isEmpty())
        return true;

    QSslConfiguration conf = m_sslSocket->sslConfiguration();
    if (!m_tlsCaCertificateFile.isEmpty()) {
        const QList<QSslCertificate> cas = loadCertificates(m_tlsCaCertificateFile);
        if (cas.isEmpty()) {
            *error = QString("Cannot load CA certificate file: %1").arg(m_tlsCaCertificateFile);
            return false;
        }
        conf.addCaCertificates(cas);
    }
    if (!m_tlsClientCertificateFile.isEmpty()) {
        const QList<QSslCertificate> certs = loadCertificates(m_tlsClientCertificateFile);
        if (certs.isEmpty()) {
            *error = QString("Cannot load client certificate file: %1")
                         .arg(m_tlsClientCertificateFile);
            return false;
        }
        const QString keyPath = m_tlsClientKeyFile.isEmpty() ? m_tlsClientCertificateFile
                                                             : m_tlsClientKeyFile;
        const QSslKey key = loadPrivateKey(keyPath);
        if (key.isNull()) {
            *error = QString("Cannot load the client private key from %1 "
                             "(passphrase-protected keys are not supported)").arg(keyPath);
            return false;
        }
        // Any further certificates in the file form the chain sent to the host
        conf.setLocalCertificateChain(certs);
        conf.setPrivateKey(key);
    }
    m_sslSocket->setSslConfiguration(conf);
#else
    Q_UNUSED(error);
#endif
    return true;
}

} // namespace tn5250::client
