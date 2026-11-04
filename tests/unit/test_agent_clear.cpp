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
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/QAgentPanelWidget/QAgentPanelWidget.h"
#include <QPushButton>
#include <QStandardPaths>
#include <QtTest/QtTest>

// Records what the panel asks of the provider; nothing goes on the network
class FakeProvider : public agent::Provider {
  public:
    QStringList resultIds;
    int cancels = 0;

    QString id() const override { return QStringLiteral("fake"); }
    QString displayName() const override { return QStringLiteral("Fake"); }
    QStringList availableModels() const override { return {}; }
    void sendMessage(const QString &, const QString &) override {}
    void sendToolResult(const agent::ToolResult &result) override { resultIds << result.toolCallId; }
    void clearHistory() override {}
    void cancel() override { ++cancels; }
    bool isConfigured() const override { return true; }
    void setApiKey(const QString &) override {}
    void setModel(const QString &) override {}
    QString apiKey() const override { return {}; }
    QString model() const override { return {}; }
};

// Regression test for issue #261: Clear did not cancel the provider's
// request in flight, and stopping a running script sent its tool result
// into the cleared conversation, because the pending call was reset only
// after the runner had reported finished.
class TestAgentClear : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void clearDuringScriptSendsNothing() {
        ui::widgets::Q5250ScreenWidget screen;
        screen.setScreenSize(24, 80);
        ui::widgets::QAgentPanelWidget panel;
        FakeProvider provider;
        panel.setProvider(&provider);
        panel.setDisplayWidget(&screen);

        agent::ToolCall call;
        call.id = QStringLiteral("toolu_1");
        call.name = agent::kToolRunScript;
        call.arguments = QStringLiteral("{\"script\":\"WAIT 2000\"}");
        emit provider.toolCallReceived(call);

        QPushButton *run = findButton(panel, QStringLiteral("Run Script"));
        QPushButton *clear = findButton(panel, QStringLiteral("Clear"));
        QVERIFY(run && clear);
        run->click(); // the script now waits 2 s
        QTest::qWait(100);

        clear->click();
        QTest::qWait(2500);
        QVERIFY2(provider.resultIds.isEmpty(), qPrintable(provider.resultIds.join(',')));
        QVERIFY(provider.cancels >= 1);
    }

  private:
    static QPushButton *findButton(QWidget &parent, const QString &text) {
        for (QPushButton *b : parent.findChildren<QPushButton *>())
            if (b->text() == text) return b;
        return nullptr;
    }
};

QTEST_MAIN(TestAgentClear)
#include "test_agent_clear.moc"
