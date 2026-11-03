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

#include "agent/auth/oauth_flow.h"
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QtTest/QtTest>

using agent::OAuthFlow;

// The local OAuth redirect listener must check the callback before telling
// the browser anything, ignore requests that are not the callback, and
// read a request that arrives in several TCP segments.
class TestOAuthCallback : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        agent::OAuthConfig config;
        config.authorizationEndpoint = QStringLiteral("https://auth.example/authorize");
        config.clientId = QStringLiteral("client");
        // Token endpoint: a local server answering with a token
        QVERIFY(m_tokenServer.listen(QHostAddress::LocalHost, 0));
        connect(&m_tokenServer, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *s = m_tokenServer.nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, s, [s]() {
                    s->readAll();
                    const QByteArray body =
                        "{\"access_token\":\"TOKEN\",\"refresh_token\":\"R\",\"expires_in\":60}";
                    s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                             + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    s->disconnectFromHost();
                });
            }
        });
        config.tokenEndpoint = QStringLiteral("http://127.0.0.1:%1/token").arg(m_tokenServer.serverPort());
        m_flow = new OAuthFlow(config);
        m_flow->setUrlOpener([this](const QUrl &url) {
            const QUrlQuery q(url);
            m_state = q.queryItemValue("state");
            m_port = QUrl(q.queryItemValue("redirect_uri", QUrl::FullyDecoded)).port();
            return true;
        });
        m_failed = new QSignalSpy(m_flow, &OAuthFlow::flowFailed);
        m_completed = new QSignalSpy(m_flow, &OAuthFlow::flowCompleted);
        m_flow->start();
        QVERIFY(m_port > 0);
        QVERIFY(!m_state.isEmpty());
    }

    void cleanup() {
        delete m_failed;
        delete m_completed;
        delete m_flow;
        m_tokenServer.close();
    }

    void otherPathsAreIgnored() {
        QVERIFY(get({"GET /favicon.ico HTTP/1.1\r\n\r\n"}).startsWith("HTTP/1.1 404"));
        QCOMPARE(m_failed->size(), 0);
        // The real callback still works afterwards
        QVERIFY(get({callback("code=abc")}).contains("Authorization received"));
        QTRY_COMPARE_WITH_TIMEOUT(m_completed->size(), 1, 5000);
    }

    void wrongStateIsRefusedBeforeAnySuccessPage() {
        const QByteArray reply = get({"GET /callback?code=abc&state=forged HTTP/1.1\r\n\r\n"});
        QVERIFY(reply.startsWith("HTTP/1.1 400"));
        QVERIFY(!reply.contains("successful") && !reply.contains("received"));
        QCOMPARE(m_failed->size(), 0); // keeps waiting for the real callback
        QVERIFY(get({callback("code=abc")}).contains("Authorization received"));
        QTRY_COMPARE_WITH_TIMEOUT(m_completed->size(), 1, 5000);
        QCOMPARE(m_completed->first().at(0).toString(), QStringLiteral("TOKEN"));
    }

    void errorIsReportedToTheBrowser() {
        const QByteArray reply =
            get({callback("error=access_denied&error_description=User%20cancelled")});
        QVERIFY(reply.contains("Authentication failed"));
        QVERIFY(reply.contains("User cancelled"));
        QVERIFY(!reply.contains("successful"));
        QCOMPARE(m_failed->size(), 1);
    }

    void requestSplitAcrossReadsIsParsed() {
        const QByteArray line = callback("code=abc");
        const QByteArray reply = get({line.left(10), line.mid(10, 25), line.mid(35)});
        QCOMPARE(m_failed->size(), 0); // not parsed from the first segment alone
        QTRY_COMPARE_WITH_TIMEOUT(m_completed->size(), 1, 5000);
        QVERIFY2(reply.contains("Authorization received"), reply.constData());
    }

  private:
    QByteArray callback(const QByteArray &params) const {
        return "GET /callback?" + params + "&state="
               + QUrl::toPercentEncoding(m_state) + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    }

    // Sends the request in the given pieces and returns the full reply.
    QByteArray get(const QList<QByteArray> &pieces) {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, m_port);
        if (!socket.waitForConnected(5000))
            return QByteArray();
        for (const QByteArray &piece : pieces) {
            socket.write(piece);
            socket.flush();
            QTest::qWait(30);
        }
        QByteArray reply;
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 5000 && socket.state() == QAbstractSocket::ConnectedState) {
            QTest::qWait(10);
            reply += socket.readAll();
        }
        reply += socket.readAll();
        return reply;
    }

    QTcpServer m_tokenServer;
    OAuthFlow *m_flow = nullptr;
    QSignalSpy *m_failed = nullptr;
    QSignalSpy *m_completed = nullptr;
    QString m_state;
    int m_port = 0;
};

QTEST_MAIN(TestOAuthCallback)
#include "test_oauth_callback.moc"
