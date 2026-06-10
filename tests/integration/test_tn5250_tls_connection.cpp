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

#include "network/tn5250_qt/client/client.h"

#include <QtTest/QtTest>

#ifdef HAVE_QT6_SSL
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>
#endif

using namespace tn5250::client;

namespace {

// Self-signed CN=localhost certificate generated for this test only
// (valid until 2126). Never use outside the test suite.
const char kTestCertPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDCzCCAfOgAwIBAgIUWJcvbMjMhEV+CDg0upy/AtLIHicwDQYJKoZIhvcNAQEL\n"
    "BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MDYxMDA3MDQyOFoYDzIxMjYw\n"
    "NTE3MDcwNDI4WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEB\n"
    "AQUAA4IBDwAwggEKAoIBAQDSZKcCKrhMXhjM64KYoF9leK2IRpNnbFWU5Q7DFeRM\n"
    "p9I4oRxqrvzM3zPm0V0WqGL2m/7xoHIvxGKNXMhCqntr8Qr7q0YC5moxNhtgVMB4\n"
    "NPgcpWwSd1GsuD8OAUooSKwq/qxLeo4VAXscrgXXXgqDPTz177LXw2umE7Se+ejH\n"
    "8u3+JrKG+bgqTDiuUGEBCbYtLvv47h2Cw2XBNCSPth1pwniBfs/+ocgHPC9Xco//\n"
    "woFuDzw0hGIpzzbVi36uHqBYz0VGz22Us+bCue9ZvZ+plsxIIbvf8W8uF8gyxPOQ\n"
    "2mPCeH01JZ3uXA/ArdcAMTj26uPkRBS/+UNciR8jXW8JAgMBAAGjUzBRMB0GA1Ud\n"
    "DgQWBBSo26+Aq/IqhqzK5KDYsJyIdbcgizAfBgNVHSMEGDAWgBSo26+Aq/IqhqzK\n"
    "5KDYsJyIdbcgizAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQA5\n"
    "8TlyvyplsZYunuAh/cBni/EuiWCVzphR9/JrM3usvZdMPwT6iZ/uHLViD65AqNxT\n"
    "D9/pBr/0MiAYSFKxcxpFmMD+rV5rgIwlqGfmBG43ArztPQx5eM/EsUnfbv7A8EwO\n"
    "BPVTn7UICnfrQnxcu79Gzz+btbRcvIDjVTZFmIFz/YJ0r4U+mKmObmC2Y7gy0ZOH\n"
    "ZD/kAqJSdC3u6NpRf8FZJIzTPCidfRNkW+eJYdJwligh8NGua4qit7v1hIVsj4ax\n"
    "4TrTUf+J0KhgyQR0g4vpcX2CLLDKiGip6EW6hvp4nNIAf6VyUTY7o+c1l8wsgqOp\n"
    "g84HV/VzZnCOFxqybc4k\n"
    "-----END CERTIFICATE-----\n";

const char kTestKeyPem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQDSZKcCKrhMXhjM\n"
    "64KYoF9leK2IRpNnbFWU5Q7DFeRMp9I4oRxqrvzM3zPm0V0WqGL2m/7xoHIvxGKN\n"
    "XMhCqntr8Qr7q0YC5moxNhtgVMB4NPgcpWwSd1GsuD8OAUooSKwq/qxLeo4VAXsc\n"
    "rgXXXgqDPTz177LXw2umE7Se+ejH8u3+JrKG+bgqTDiuUGEBCbYtLvv47h2Cw2XB\n"
    "NCSPth1pwniBfs/+ocgHPC9Xco//woFuDzw0hGIpzzbVi36uHqBYz0VGz22Us+bC\n"
    "ue9ZvZ+plsxIIbvf8W8uF8gyxPOQ2mPCeH01JZ3uXA/ArdcAMTj26uPkRBS/+UNc\n"
    "iR8jXW8JAgMBAAECggEAA0uZymLyrJmJbxKAHpnmcfGswTRaczFLx2TcdWngN0lZ\n"
    "jzloP9GWIPeLzyWvPP06sMi0re0STD/M+qF5roJLo9JbVpC++Slr90b9ffJ7v7qC\n"
    "7b4zWa+xcoO5QgpIB/NlLXripjLdU6zd4SAq/K/r5+5bdyj/tZ+8q55uj6d8X7aI\n"
    "dCEAzmYRpZfW6zgrayv3/fOUg7XBmxQxhAiH4UG5e6lz+8PuLuNfko2K0SyosoP1\n"
    "y1KhiXqGQ5pvPJ64MG40VHseMby/jadr7BT5R24UCUyZ8vgB/B8BPwPV3yqNXVnq\n"
    "LJyLNH0fWPWYghlV938RO0jHogQyMOkquypK3zz8/QKBgQD6w/bU9p1H+BZxqyRW\n"
    "FW+XLw5DBsqu+fK4kl0Bs2kiu94U9xI8SymmSl5sU0wfvftvWqeiCDU7V+KyzlYu\n"
    "XHz2ZkDUqKvrfOXsdrvih47vKxF3X/7ZH1afd8/lnRu9sylnk/NB/EFwip0UrTP1\n"
    "7qyBu+DI89NlA0uPaNJQOULHlQKBgQDWyPKI2ztHM2xL3fxnIMa3wkJXUYOwstJh\n"
    "zP7FJBV2VTCdC/Gt9j5C2dRwDx32QlqAl1p4GWGiNr4DquBeEaFGUVkOA42OuFrX\n"
    "kVRUa+Qg8I0xM2ocs1CFQzsoIuiFHmbt3CHkovb230ouMwJrSrZ+AanMCBJL/m34\n"
    "1dOWZeScpQKBgBqaudBETdF535+1oYhEg+9NPb0ctlo0CG1OkfGBQFFAD0K4J8Yf\n"
    "z05mK3hgqf3gIRHiU1CcgFFIdLO1smz+wP8/P/eP4ZV9TcN1oV9aNG7padP5akdM\n"
    "zNrkUjkxHuVUYbssdi10/thazGmKKq4X4VNuRF3tiGr6G4UegNmkCZK1AoGAXDJ+\n"
    "CckxtOqZ/icYBZzIMHEu0RSoltzr+hdo9W7714PSDlfmMmqVZ1TiIAgdMGxjNPfD\n"
    "WfJrOpqNDj33eenPdMPOmnlj9nOkawxzSpnVn14i/Y+4aQF/+vRVHHF/pkTaohfw\n"
    "ZJifsnE/An3a9/tmQsir/m0ojX517m67GMA8VhECgYAKB6xYXcP706yFeu0l2/MR\n"
    "NsUvNEWbSSxRGDzAauWravdese3TqN+fBP2NpDrSNU8Did1CxEKQtBKpIuozMYdU\n"
    "2SAMHArX1mcmgaO6r1dNowEBPNKKKj44vBpzT0vlYr/uAtkdRv7U7Kg5jHUXeYdz\n"
    "yHDjiGok8w3AkYkBK9mzHg==\n"
    "-----END PRIVATE KEY-----\n";

} // namespace

// Regression test for issue #135: implicit-TLS sessions never started the
// telnet negotiation because QSslSocket::encrypted was not connected, so the
// post-encryption branch of onSocketConnected() was unreachable.
class TestTN5250TlsConnection : public QObject {
    Q_OBJECT

  private slots:
    void testTlsHandshakeStartsTelnetNegotiation();
};

void TestTN5250TlsConnection::testTlsHandshakeStartsTelnetNegotiation() {
#ifndef HAVE_QT6_SSL
    QSKIP("Built without Qt6 SSL support");
#else
    if (!QSslSocket::supportsSsl()) {
        QSKIP("No SSL backend available at runtime");
    }

    QSslServer server;
    QSslConfiguration serverConfig = QSslConfiguration::defaultConfiguration();
    serverConfig.setLocalCertificate(QSslCertificate(QByteArray(kTestCertPem)));
    serverConfig.setPrivateKey(
        QSslKey(QByteArray(kTestKeyPem), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey));
    QVERIFY(!serverConfig.localCertificate().isNull());
    QVERIFY(!serverConfig.privateKey().isNull());
    server.setSslConfiguration(serverConfig);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QByteArray serverReceived;
    connect(&server, &QTcpServer::pendingConnectionAvailable, this, [&]() {
        QTcpSocket *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [&serverReceived, socket]() {
            serverReceived.append(socket->readAll());
        });
    });

    TN5250Client client;
    client.setAllowInvalidCertificates(true); // self-signed test certificate
    client.connectToHost(QStringLiteral("127.0.0.1"), server.serverPort(), true);

    // After the TLS handshake completes the client must proactively send its
    // RFC 1205 option negotiation (DO/WILL BINARY, DO/WILL EOR = four 3-byte
    // telnet commands). Without the encrypted() connection nothing is ever
    // sent and the state stays Connecting.
    QTRY_VERIFY_WITH_TIMEOUT(serverReceived.size() >= 12, 5000);
    QCOMPARE(static_cast<quint8>(serverReceived[0]), quint8(0xFF)); // IAC

    client.disconnectFromHost();
#endif
}

QTEST_MAIN(TestTN5250TlsConnection)
#include "test_tn5250_tls_connection.moc"
