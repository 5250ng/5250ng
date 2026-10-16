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

#include <QTemporaryDir>
#include <QtTest/QtTest>

#ifdef HAVE_QT6_SSL
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
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

#ifdef HAVE_QT6_SSL
// Short-lived PKI generated per run: a CA and a localhost certificate it
// signed, usable for both server and client authentication. Generated at
// runtime because some TLS backends (macOS Secure Transport) refuse a
// self-signed CA certificate as a server certificate and cap the lifetime
// of certificates chained to user-supplied anchors, so a fixed long-lived
// fixture cannot be validated there.
struct TestPki {
    QByteArray caPem;
    QByteArray leafPem;
    QByteArray leafKeyPem;
};

EVP_PKEY *generateKey() {
    EVP_PKEY *key = nullptr;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (ctx && EVP_PKEY_keygen_init(ctx) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) > 0)
        EVP_PKEY_keygen(ctx, &key);
    EVP_PKEY_CTX_free(ctx);
    return key;
}

void addExtension(X509 *cert, X509 *issuer, int nid, const char *value) {
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, issuer, cert, nullptr, nullptr, 0);
    X509_EXTENSION *ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
    if (ext) {
        X509_add_ext(cert, ext, -1);
        X509_EXTENSION_free(ext);
    }
}

// issuer/issuerKey null = self-signed CA
X509 *makeCertificate(EVP_PKEY *key, const char *cn, long serial,
                      X509 *issuer, EVP_PKEY *issuerKey) {
    X509 *cert = X509_new();
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), serial);
    X509_gmtime_adj(X509_getm_notBefore(cert), -3600);
    X509_gmtime_adj(X509_getm_notAfter(cert), 24 * 3600);
    X509_set_pubkey(cert, key);
    X509_NAME *name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char *>(cn), -1, -1, 0);
    X509 *signer = issuer ? issuer : cert;
    X509_set_issuer_name(cert, X509_get_subject_name(signer));

    addExtension(cert, signer, NID_subject_key_identifier, "hash");
    addExtension(cert, signer, NID_authority_key_identifier, "keyid:always");
    if (!issuer) {
        addExtension(cert, signer, NID_basic_constraints, "critical,CA:TRUE");
        addExtension(cert, signer, NID_key_usage, "critical,keyCertSign,cRLSign");
    } else {
        addExtension(cert, signer, NID_basic_constraints, "critical,CA:FALSE");
        addExtension(cert, signer, NID_key_usage, "critical,digitalSignature,keyEncipherment");
        addExtension(cert, signer, NID_ext_key_usage, "serverAuth,clientAuth");
        addExtension(cert, signer, NID_subject_alt_name, "DNS:localhost,IP:127.0.0.1");
    }
    X509_sign(cert, issuer ? issuerKey : key, EVP_sha256());
    return cert;
}

QByteArray toPem(X509 *cert) {
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(bio, cert);
    char *data = nullptr;
    const long len = BIO_get_mem_data(bio, &data);
    QByteArray pem(data, static_cast<int>(len));
    BIO_free(bio);
    return pem;
}

QByteArray toPem(EVP_PKEY *key) {
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr);
    char *data = nullptr;
    const long len = BIO_get_mem_data(bio, &data);
    QByteArray pem(data, static_cast<int>(len));
    BIO_free(bio);
    return pem;
}

TestPki generatePki() {
    TestPki pki;
    EVP_PKEY *caKey = generateKey();
    EVP_PKEY *leafKey = generateKey();
    if (!caKey || !leafKey) {
        EVP_PKEY_free(caKey);
        EVP_PKEY_free(leafKey);
        return pki;
    }
    X509 *ca = makeCertificate(caKey, "5250ng Test CA", 1, nullptr, nullptr);
    X509 *leaf = makeCertificate(leafKey, "localhost", 2, ca, caKey);
    pki.caPem = toPem(ca);
    pki.leafPem = toPem(leaf);
    pki.leafKeyPem = toPem(leafKey);
    X509_free(leaf);
    X509_free(ca);
    EVP_PKEY_free(leafKey);
    EVP_PKEY_free(caKey);
    return pki;
}

// TLS server presenting the generated localhost certificate. With
// requireClientCert it also demands a client certificate signed by the CA.
bool startTestServer(QSslServer &server, const TestPki &pki, bool requireClientCert) {
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setLocalCertificateChain({QSslCertificate(pki.leafPem)});
    config.setPrivateKey(QSslKey(pki.leafKeyPem, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey));
    if (requireClientCert) {
        config.setPeerVerifyMode(QSslSocket::VerifyPeer);
        config.setCaCertificates({QSslCertificate(pki.caPem)});
    }
    server.setSslConfiguration(config);
    return server.listen(QHostAddress::Any, 0);
}

QString writeFile(const QTemporaryDir &dir, const QString &name, const QByteArray &data) {
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    f.write(data);
    return path;
}
#endif

} // namespace

// Regression test for issue #135: implicit-TLS sessions never started the
// telnet negotiation because QSslSocket::encrypted was not connected, so the
// post-encryption branch of onSocketConnected() was unreachable.
class TestTN5250TlsConnection : public QObject {
    Q_OBJECT

  private slots:
    void testTlsHandshakeStartsTelnetNegotiation();
    void testCustomCaCertificateIsTrusted();
    void testUntrustedCertificateRejectedWithoutCa();
    void testClientCertificateIsPresented();
    void testUnreadableCaFileFailsFast();
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

void TestTN5250TlsConnection::testCustomCaCertificateIsTrusted() {
#ifndef HAVE_QT6_SSL
    QSKIP("Built without Qt6 SSL support");
#else
    if (!QSslSocket::supportsSsl())
        QSKIP("No SSL backend available at runtime");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const TestPki pki = generatePki();
    QVERIFY(!pki.caPem.isEmpty());
    QSslServer server;
    QVERIFY(startTestServer(server, pki, false));

    TN5250Client client;
    client.setTlsCaCertificateFile(writeFile(dir, "ca.pem", pki.caPem));
    QStringList errors;
    connect(&client, &TN5250Client::errorOccurred, this,
            [&errors](const QString &e) { errors << e; });
    // Certificate validation stays on: the session-specific CA must make the
    // host certificate trusted.
    client.connectToHost(QStringLiteral("localhost"), server.serverPort(), true);
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), TN5250Client::ConnectionState::Negotiating, 5000);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join("; ")));
    client.disconnectFromHost();
#endif
}

void TestTN5250TlsConnection::testUntrustedCertificateRejectedWithoutCa() {
#ifndef HAVE_QT6_SSL
    QSKIP("Built without Qt6 SSL support");
#else
    if (!QSslSocket::supportsSsl())
        QSKIP("No SSL backend available at runtime");
    const TestPki pki = generatePki();
    QVERIFY(!pki.caPem.isEmpty());
    QSslServer server;
    QVERIFY(startTestServer(server, pki, false));

    TN5250Client client;
    QStringList errors;
    connect(&client, &TN5250Client::errorOccurred, this,
            [&errors](const QString &e) { errors << e; });
    client.connectToHost(QStringLiteral("localhost"), server.serverPort(), true);
    QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 5000);
    QVERIFY(errors.first().contains("certificate validation failed"));
    QVERIFY(client.state() != TN5250Client::ConnectionState::Negotiating);
#endif
}

void TestTN5250TlsConnection::testClientCertificateIsPresented() {
#ifndef HAVE_QT6_SSL
    QSKIP("Built without Qt6 SSL support");
#else
    if (!QSslSocket::supportsSsl())
        QSKIP("No SSL backend available at runtime");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const TestPki pki = generatePki();
    QVERIFY(!pki.caPem.isEmpty());
    QSslServer server;
    QVERIFY(startTestServer(server, pki, true));

    QSslCertificate seenByServer;
    connect(&server, &QTcpServer::pendingConnectionAvailable, this, [&]() {
        auto *socket = qobject_cast<QSslSocket *>(server.nextPendingConnection());
        if (socket)
            seenByServer = socket->peerCertificate();
    });

    TN5250Client client;
    client.setTlsCaCertificateFile(writeFile(dir, "ca.pem", pki.caPem));
    // Combined PEM: the key is read from the certificate file
    client.setTlsClientCertificate(
        writeFile(dir, "client.pem", pki.leafPem + pki.leafKeyPem), QString());
    client.connectToHost(QStringLiteral("localhost"), server.serverPort(), true);
    QTRY_VERIFY_WITH_TIMEOUT(!seenByServer.isNull(), 5000);
    QCOMPARE(seenByServer, QSslCertificate(pki.leafPem));
    client.disconnectFromHost();
#endif
}

void TestTN5250TlsConnection::testUnreadableCaFileFailsFast() {
#ifndef HAVE_QT6_SSL
    QSKIP("Built without Qt6 SSL support");
#else
    TN5250Client client;
    client.setTlsCaCertificateFile(QStringLiteral("/nonexistent/ca.pem"));
    QStringList errors;
    connect(&client, &TN5250Client::errorOccurred, this,
            [&errors](const QString &e) { errors << e; });
    client.connectToHost(QStringLiteral("127.0.0.1"), 1, true);
    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.first().contains("Cannot load CA certificate file"));
    QCOMPARE(client.state(), TN5250Client::ConnectionState::Error);
#endif
}

QTEST_MAIN(TestTN5250TlsConnection)
#include "test_tn5250_tls_connection.moc"
