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

#include "session/config.h"
#include "session/manager.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest/QtTest>

// Two tabs of the same profile on one host get distinct device names
// (MYTERM, MYTERM1). Session > Save must write the configured name back,
// not the one the tab was given to avoid the clash.
class TestSaveSessionDeviceName : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void saveKeepsConfiguredDeviceName() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        int connections = 0;
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                ++connections;
                socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
            }
        });

        session::SessionConfig config;
        config.setName(QStringLiteral("devname-test"));
        config.setHostname(QStringLiteral("127.0.0.1"));
        config.setPort(server.serverPort());
        config.setDeviceName(QStringLiteral("MYTERM"));
        session::SessionManager mgr;
        QVERIFY(mgr.saveSession(config));

        MainWindow window;
        window.autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);
        window.autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 2, 15000);
        auto *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        tabs->setCurrentIndex(1); // the tab running as MYTERM1

        QTimer dismiss;
        connect(&dismiss, &QTimer::timeout, this, []() {
            if (QWidget *modal = QApplication::activeModalWidget()) {
                for (QPushButton *b : modal->findChildren<QPushButton *>())
                    if (b->text() == "OK") { b->click(); return; }
            }
        });
        dismiss.start(20);
        QVERIFY(QMetaObject::invokeMethod(&window, "onSaveSession"));
        QTRY_VERIFY_WITH_TIMEOUT(!QApplication::activeModalWidget(), 5000);

        // Reconnecting the second tab keeps the configured name as well
        QVERIFY(QMetaObject::invokeMethod(&window, "onReconnect"));
        QTRY_COMPARE_WITH_TIMEOUT(connections, 3, 15000);
        QCOMPARE(tabs->count(), 2);
        tabs->setCurrentIndex(tabs->count() - 1);
        QVERIFY(QMetaObject::invokeMethod(&window, "onSaveSession"));
        dismiss.stop();

        session::SessionManager reader;
        session::SessionConfig saved;
        QVERIFY(reader.loadSession(QStringLiteral("devname-test"), saved));
        QCOMPARE(saved.deviceName(), QStringLiteral("MYTERM"));
        QVERIFY(reader.deleteSession(QStringLiteral("devname-test")));
    }
};

QTEST_MAIN(TestSaveSessionDeviceName)
#include "test_save_session_device_name.moc"
