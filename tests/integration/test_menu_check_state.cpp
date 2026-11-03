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
#include "ui/main_window.h"

#include <QAction>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Toggle items for per-session state (View > Hotspots, Tools > Show cursor
// rules, ...) must show the active tab's state, and be disabled without a
// session.
class TestMenuCheckState : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void togglesFollowTheActiveTab() {
        QTcpServer host;
        QVERIFY(host.listen(QHostAddress::LocalHost, 0));
        int connections = 0;
        connect(&host, &QTcpServer::newConnection, this, [&]() {
            while (QTcpSocket *socket = host.nextPendingConnection()) {
                ++connections;
                socket->write(QByteArray::fromHex("fffb00fffd00fffa1801fff0"));
            }
        });
        session::SessionConfig config;
        config.setName(QStringLiteral("menu-sync"));
        config.setHostname(QStringLiteral("127.0.0.1"));
        config.setPort(host.serverPort());

        MainWindow window;
        QAction *rules = action(window, "Show &cursor rules");
        QVERIFY(rules);
        QVERIFY(!rules->isEnabled()); // no session yet

        window.autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);
        window.autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 2, 15000);
        auto *tabs = window.findChild<QTabWidget *>();
        QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 2, 5000);

        tabs->setCurrentIndex(0);
        QVERIFY(rules->isEnabled());
        QVERIFY(action(window, "&Hotspots")->isEnabled());
        QVERIFY(!rules->isChecked());
        rules->trigger(); // turn cursor rules on in tab 1 only
        QVERIFY(rules->isChecked());

        tabs->setCurrentIndex(1);
        QVERIFY2(!rules->isChecked(), "tab 2 shows tab 1's state");
        tabs->setCurrentIndex(0);
        QVERIFY(rules->isChecked());

        // With every tab closed, the items cannot act on anything
        while (tabs->count() > 0)
            QMetaObject::invokeMethod(&window, "onCloseTabRequested", Q_ARG(int, 0));
        QVERIFY(!rules->isEnabled());
        QVERIFY(!rules->isChecked());
        QAction *hotspots = action(window, "&Hotspots");
        QVERIFY(hotspots && !hotspots->isEnabled());
    }

  private:
    static QAction *action(MainWindow &window, const QString &text) {
        for (QAction *a : window.findChildren<QAction *>())
            if (a->text() == text)
                return a;
        return nullptr;
    }
};

QTEST_MAIN(TestMenuCheckState)
#include "test_menu_check_state.moc"
