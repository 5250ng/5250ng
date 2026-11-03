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

#include "agent/provider.h"
#include "agent/tool_definitions.h"
#include "session/config.h"
#include "ui/main_window.h"
#include "ui/themes/manager.h"
#include "ui/widgets/QAgentPanelWidget/QAgentPanelWidget.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"

#include <QPointer>

#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

// Stands in for an LLM provider; records nothing and sends nothing
class FakeProvider : public agent::Provider {
  public:
    QString id() const override { return QStringLiteral("fake"); }
    QString displayName() const override { return QStringLiteral("Fake"); }
    QStringList availableModels() const override { return {}; }
    void sendMessage(const QString &, const QString &) override {}
    void sendToolResult(const agent::ToolResult &) override {}
    void clearHistory() override {}
    void cancel() override {}
    bool isConfigured() const override { return true; }
    void setApiKey(const QString &) override {}
    void setModel(const QString &) override {}
    QString apiKey() const override { return {}; }
    QString model() const override { return {}; }
    void callRunScript() {
        agent::ToolCall call;
        call.id = QStringLiteral("toolu_1");
        call.name = agent::kToolRunScript;
        call.arguments = QStringLiteral("{\"script\":\"TYPE \\\"x\\\"\"}");
        emit toolCallReceived(call);
    }
};

// Regression test: closing a tab cut every connection of its screen widget,
// including the destroyed() link Qt's style sheet style uses to drop the
// widget from its cache, so the next application style sheet change (a
// theme switch) used the deleted widget and crashed.
class TestCloseTabThemeChange : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        ui::themes::ThemeManager::instance().loadBuiltinThemes();
    }

    void themeChangeAfterClosingTab() { run(false); }

    // The closed tab's agent panel had wired its tool-call buttons, which
    // also cut every connection of those buttons
    void themeChangeAfterClosingTabWithAgentPrompt() { run(true); }

  private:
    void run(bool agentPrompt) {
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
        config.setName(QStringLiteral("theme-crash"));
        config.setHostname(QStringLiteral("127.0.0.1"));
        config.setPort(host.serverPort());

        MainWindow window;
        window.show();
        window.autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 1, 15000);
        auto *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 1, 5000);
        // The first tab's widgets, before the second tab adds its own
        QPointer<ui::widgets::Q5250ScreenWidget> closedScreen =
            window.findChild<ui::widgets::Q5250ScreenWidget *>();
        QPointer<ui::widgets::QAgentPanelWidget> closedPanel =
            window.findChild<ui::widgets::QAgentPanelWidget *>();
        QVERIFY(closedScreen && closedPanel);
        FakeProvider provider;
        if (agentPrompt) {
            closedPanel->setProvider(&provider);
            closedPanel->show();
            closedPanel->ensurePolished();
            provider.callRunScript(); // shows the Run Script / Cancel prompt
        }
        window.autoConnect(config);
        QTRY_COMPARE_WITH_TIMEOUT(connections, 2, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 2, 5000);

        QVERIFY(QMetaObject::invokeMethod(&window, "onCloseTabRequested", Q_ARG(int, 0)));
        QTRY_COMPARE_WITH_TIMEOUT(tabs->count(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(closedScreen.isNull() && closedPanel.isNull(), 5000);

        // Switch the application theme both ways, as Settings does
        auto &themes = ui::themes::ThemeManager::instance();
        for (const QString &name : themes.availableThemes())
            QVERIFY(themes.setCurrentTheme(name));
        QVERIFY(themes.setCurrentTheme(QStringLiteral("dark")));
        QCOMPARE(tabs->count(), 1);
    }
};

QTEST_MAIN(TestCloseTabThemeChange)
#include "test_close_tab_theme_change.moc"
