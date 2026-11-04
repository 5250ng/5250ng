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
#include "core/ebcdic.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/QAgentPanelWidget/QAgentPanelWidget.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// Records the tool results the panel sends; nothing goes on the network
class FakeProvider : public agent::Provider {
  public:
    QList<agent::ToolResult> results;
    QString id() const override { return QStringLiteral("fake"); }
    QString displayName() const override { return QStringLiteral("Fake"); }
    QStringList availableModels() const override { return {}; }
    void sendMessage(const QString &, const QString &) override {}
    void sendToolResult(const agent::ToolResult &r) override { results << r; }
    void clearHistory() override {}
    void cancel() override {}
    bool isConfigured() const override { return true; }
    void setApiKey(const QString &) override {}
    void setModel(const QString &) override {}
    QString apiKey() const override { return {}; }
    QString model() const override { return {}; }
};

using ui::widgets::KeyboardState;

// Regression tests for issue #262: the agent panel's send_keys, type_text
// and file tools lacked the MCP handlers' fixes, so model text could break
// or inject 5250Script, UP/DOWN/LEFT/RIGHT were rejected, and file writes
// were translated and unchecked.
class TestAgentToolEscaping : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init();
    void cleanup();

    void typeTextIsTypedLiterally();
    void sendKeysQuotedTextAndArrowKeys();
    void writeFileIsByteExact();
    void writeFileWithoutContentIsRefused();
    void typeTextNewlineDoesNotInjectStatements();
    void waitForTextMatchesLiterally();

  private:
    // Sends a tool call, approves it (Run Script / Accept) and waits for its result
    agent::ToolResult runTool(const QString &name, const QJsonObject &args);
    QString fieldText() const;

    ui::widgets::Q5250ScreenWidget *m_screen = nullptr;
    ui::widgets::QAgentPanelWidget *m_panel = nullptr;
    FakeProvider *m_provider = nullptr;
    int m_callNo = 0;
};

void TestAgentToolEscaping::init() {
    m_screen = new ui::widgets::Q5250ScreenWidget();
    m_screen->setScreenSize(24, 80);
    m_screen->setKeyboardState(KeyboardState::Unlocked);
    auto *buf = m_screen->screenBuffer();
    buf->setField(4, 2, 40, false);
    buf->setField(5, 2, 40, false);
    m_screen->moveCursor(5, 2);
    m_panel = new ui::widgets::QAgentPanelWidget();
    m_provider = new FakeProvider();
    m_panel->setProvider(m_provider);
    m_panel->setDisplayWidget(m_screen);
}

void TestAgentToolEscaping::cleanup() {
    delete m_panel;
    delete m_provider;
    delete m_screen;
}

agent::ToolResult TestAgentToolEscaping::runTool(const QString &name, const QJsonObject &args) {
    agent::ToolCall call;
    call.id = QStringLiteral("toolu_%1").arg(++m_callNo);
    call.name = name;
    call.arguments = QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact));
    const int before = m_provider->results.size();
    emit m_provider->toolCallReceived(call);
    for (QPushButton *b : m_panel->findChildren<QPushButton *>()) {
        static const QStringList approve = {"Run Script", "Type Text", "Send Keys", "Write File", "Wait for Text"};
        if (approve.contains(b->text()) && b->isEnabled()) {
            b->click();
            break;
        }
    }
    [&]() { QTRY_VERIFY_WITH_TIMEOUT(m_provider->results.size() > before, 5000); }();
    return m_provider->results.size() > before ? m_provider->results.last() : agent::ToolResult{};
}

QString TestAgentToolEscaping::fieldText() const {
    QString t;
    for (int c = 2; c < 42; ++c) {
        const uint8_t ch = m_screen->screenBuffer()->character(5, c);
        t += (ch == 0x00) ? QChar(' ') : core::EBCDIC::ebcdicToChar(ch);
    }
    while (t.endsWith(' ')) t.chop(1);
    return t;
}

void TestAgentToolEscaping::typeTextIsTypedLiterally() {
    // A quote, a backslash (C:\new would become a newline) and a variable
    const QString text = QStringLiteral("a\"b C:\\new $HOME");
    const agent::ToolResult r = runTool(agent::kToolTypeText, {{"text", text}});
    QVERIFY2(r.success, qPrintable(r.output));
    QCOMPARE(fieldText(), text);
}

void TestAgentToolEscaping::sendKeysQuotedTextAndArrowKeys() {
    // The keys string is "x\y" UP: the quoted text holds two backslashes
    const agent::ToolResult r = runTool(agent::kToolSendKeys, {{"keys", "\"x\\\\y\" UP"}});
    QVERIFY2(r.success, qPrintable(r.output));
    QCOMPARE(fieldText(), QStringLiteral("x\\\\y"));
    QCOMPARE(m_screen->screenBuffer()->cursorPosition().y(), 4); // UP moved the cursor
}

void TestAgentToolEscaping::writeFileIsByteExact() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("out.txt"));
    const QString content = QStringLiteral("line1\nline2\n");
    const agent::ToolResult r = runTool(agent::kToolWriteFile, {{"path", path}, {"content", content}});
    QVERIFY2(r.success, qPrintable(r.output));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), content.toUtf8()); // no CRLF translation on Windows
}

void TestAgentToolEscaping::writeFileWithoutContentIsRefused() {
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("keep.txt"));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("keep");
    }
    agent::ToolCall call;
    call.id = QStringLiteral("toolu_nc");
    call.name = agent::kToolWriteFile;
    call.arguments = QString::fromUtf8(QJsonDocument(QJsonObject{{"path", path}}).toJson());
    emit m_provider->toolCallReceived(call);
    QTRY_VERIFY_WITH_TIMEOUT(!m_provider->results.isEmpty(), 3000);
    QVERIFY(!m_provider->results.last().success);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("keep"));
}

void TestAgentToolEscaping::typeTextNewlineDoesNotInjectStatements() {
    // A line break must not end TYPE and start a statement of its own
    QSignalSpy sent(m_screen, &ui::widgets::Q5250ScreenWidget::inputReady);
    const agent::ToolResult r = runTool(agent::kToolTypeText, {{"text", "a\nPRESS ENTER"}});
    QVERIFY2(r.success, qPrintable(r.output));
    QTest::qWait(50);
    QCOMPARE(sent.count(), 0); // no Enter AID reached the host
    QVERIFY(fieldText().startsWith('a'));
}

void TestAgentToolEscaping::waitForTextMatchesLiterally() {
    // Screen text with $ and \: EXPECT TEXT compares verbatim
    auto *buf = m_screen->screenBuffer();
    const QString shown = QStringLiteral("Total $5 C:\\X");
    for (int i = 0; i < shown.size(); ++i)
        buf->writeChar(10, 2 + i, core::EBCDIC::charToEBCDIC(shown[i]));
    const agent::ToolResult r = runTool(agent::kToolWaitForText, {{"text", shown}, {"timeout", 2000}});
    QVERIFY2(r.success, qPrintable(r.output));
}

QTEST_MAIN(TestAgentToolEscaping)
#include "test_agent_tool_escaping.moc"
