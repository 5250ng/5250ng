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
#include "agent/script_generator.h"
#include "agent/tool_definitions.h"
#include "ui/widgets/QAgentPanelWidget/QAgentPanelWidget.h"
#include <QFile>
#include <QStandardPaths>
#include <QtTest/QtTest>

// Records the tool results the panel sends back; nothing goes on the network
class FakeProvider : public agent::Provider {
  public:
    QStringList resultIds;
    // When set, answering a tool call starts the next generation at once,
    // as a provider replying with another generate_5250script call would
    QStringList chainedIds;

    QString id() const override { return QStringLiteral("fake"); }
    QString displayName() const override { return QStringLiteral("Fake"); }
    QStringList availableModels() const override { return {}; }
    void sendMessage(const QString &, const QString &) override {}
    void sendToolResult(const agent::ToolResult &result) override {
        resultIds << result.toolCallId;
        if (!chainedIds.isEmpty())
            callGenerate(chainedIds.takeFirst());
    }
    void clearHistory() override {}
    void cancel() override {}
    bool isConfigured() const override { return true; }
    void setApiKey(const QString &) override {}
    void setModel(const QString &) override {}
    QString apiKey() const override { return {}; }
    QString model() const override { return {}; }

    void callGenerate(const QString &id) {
        agent::ToolCall call;
        call.id = id;
        call.name = agent::kToolGenerateScript;
        call.arguments = QStringLiteral("{\"task\":\"sign on\"}");
        emit toolCallReceived(call);
    }
};

// Regression test for issue #260: each generate_5250script call connected a
// success and an error handler but only the one that ran was removed, so the
// other answered a later generation with the earlier call's id.
class TestAgentGenerationHandlers : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        // The test relies on the prompt template being absent: with it, the
        // generator would send a real request to a provider
        QVERIFY2(!QFile::exists(QStringLiteral(":/agent/5250script_prompt.md")),
                 "agent prompt resources are linked into this test");
    }

    void eachGenerationAnsweredOnce();
    void chainedGenerationsAnsweredOnce();
};

void TestAgentGenerationHandlers::eachGenerationAnsweredOnce() {
    ui::widgets::QAgentPanelWidget panel;
    FakeProvider provider;
    panel.setProvider(&provider);

    // This test binary has no resources, so the generator fails each
    // generation at once (no prompt template): the error handler runs
    // and the success handler is left behind on main
    provider.callGenerate(QStringLiteral("toolu_1"));
    provider.callGenerate(QStringLiteral("toolu_2"));
    QCOMPARE(provider.resultIds, (QStringList{QStringLiteral("toolu_1"), QStringLiteral("toolu_2")}));

    // A late success from the generator must not answer either call again
    auto *generator = panel.findChild<agent::ScriptGeneratorSubagent *>();
    QVERIFY(generator);
    emit generator->scriptGenerated(QStringLiteral("TYPE \"x\""));
    QCOMPARE(provider.resultIds, (QStringList{QStringLiteral("toolu_1"), QStringLiteral("toolu_2")}));
}

void TestAgentGenerationHandlers::chainedGenerationsAnsweredOnce() {
    // Each answer starts the next generation from inside the handler
    ui::widgets::QAgentPanelWidget panel;
    FakeProvider provider;
    panel.setProvider(&provider);
    provider.chainedIds = {QStringLiteral("toolu_2"), QStringLiteral("toolu_3")};
    provider.callGenerate(QStringLiteral("toolu_1"));
    const QStringList expected{QStringLiteral("toolu_1"), QStringLiteral("toolu_2"),
                               QStringLiteral("toolu_3")};
    QCOMPARE(provider.resultIds, expected);
    auto *generator = panel.findChild<agent::ScriptGeneratorSubagent *>();
    QVERIFY(generator);
    emit generator->scriptGenerated(QStringLiteral("TYPE \"x\""));
    emit generator->generationError(QStringLiteral("late"));
    QCOMPARE(provider.resultIds, expected);
}

QTEST_MAIN(TestAgentGenerationHandlers)
#include "test_agent_generation_handlers.moc"
