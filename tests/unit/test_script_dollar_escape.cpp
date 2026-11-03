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

#include "core/ebcdic.h"
#include "core/macro_recorder.h"
#include "core/macro_to_script.h"
#include "mcp/McpSessionRegistry.h"
#include "mcp/McpToolHandler.h"
#include "ui/widgets/Q5250ScreenWidget/Q5250ScreenWidget.h"
#include "ui/widgets/Q5250ScreenWidget/screen_buffer.h"
#include <QApplication>
#include <QJsonArray>
#include <QStandardPaths>
#include <QTimer>
#include <QtTest/QtTest>

using ui::widgets::KeyboardState;
using ui::widgets::Q5250ScreenWidget;

// "$" in text from MCP clients and in recorded keystrokes must reach the
// host literally; the script engine would otherwise expand "$NAME" (an
// unknown name becomes "0", so "Pa$word1" was typed as "Pa0").
class TestScriptDollarEscape : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init();
    void cleanup();
    void typeTextKeepsDollar();
    void sendKeysKeepsDollar();
    void loginKeepsDollarInPassword();
    void waitForTextMatchesDollarLiterally();
    void waitForTextTimeoutMessageKeepsDollar();
    void recordedMacroKeepsDollar();

  private:
    QJsonObject call(const QString &tool, QJsonObject args);
    static QString resultText(const QJsonObject &result);
    QString fieldText(int row, int col) const;

    Q5250ScreenWidget *m_widget = nullptr;
    mcp::McpSessionRegistry *m_registry = nullptr;
    mcp::McpToolHandler *m_handler = nullptr;
    QByteArray m_sentToHost;
};

static const QString kSessionId = QStringLiteral("dollar-session");

void TestScriptDollarEscape::init() {
    m_widget = new Q5250ScreenWidget();
    m_widget->setScreenSize(24, 80);
    auto *screen = m_widget->screenBuffer();
    const QString title = QStringLiteral("Sign On");
    for (int i = 0; i < title.size(); ++i)
        screen->writeChar(0, 35 + i, core::EBCDIC::charToEBCDIC(title[i]));
    screen->setField(5, 52, 10, false);
    screen->setFieldFFW(5, 52, 0x40, 0x00);
    screen->setField(6, 52, 10, false);
    screen->setFieldFFW(6, 52, 0x40, 0x00);
    screen->setCursorPosition(5, 52);
    m_widget->setKeyboardState(KeyboardState::Unlocked);

    m_registry = new mcp::McpSessionRegistry();
    m_handler = new mcp::McpToolHandler(m_registry);
    mcp::McpSessionInfo info;
    info.sessionId = kSessionId;
    info.hostname = QStringLiteral("ibmi.example");
    m_registry->addSession(info);
    m_registry->setDisplayWidget(kSessionId, m_widget);
    m_sentToHost.clear();
    // Stand in for the host: record the AID response and unlock again.
    connect(m_widget, &Q5250ScreenWidget::inputReady, this, [this](const QByteArray &data) {
        m_sentToHost += data;
        QTimer::singleShot(0, this, [this]() {
            m_widget->setKeyboardState(KeyboardState::Unlocked);
        });
    });
}

void TestScriptDollarEscape::cleanup() {
    delete m_handler;
    delete m_registry;
    delete m_widget;
}

QJsonObject TestScriptDollarEscape::call(const QString &tool, QJsonObject args) {
    args["session_id"] = kSessionId;
    return m_handler->callTool(tool, args);
}

QString TestScriptDollarEscape::resultText(const QJsonObject &result) {
    return result.value("content").toArray().at(0).toObject().value("text").toString();
}

QString TestScriptDollarEscape::fieldText(int row, int col) const {
    auto *screen = m_widget->screenBuffer();
    return core::EBCDIC::ebcdicToString(screen->getFieldData(screen->getField(row, col)))
        .remove(QChar(0)).trimmed();
}

void TestScriptDollarEscape::typeTextKeepsDollar() {
    const QJsonObject result = call("type_text", {{"text", "Pa$word1"}});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QCOMPARE(fieldText(5, 52), QString("Pa$word1"));
}

void TestScriptDollarEscape::sendKeysKeepsDollar() {
    const QJsonObject result = call("send_keys", {{"keys", "\"A$B$$C\""}});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QCOMPARE(fieldText(5, 52), QString("A$B$$C"));
}

void TestScriptDollarEscape::loginKeepsDollarInPassword() {
    const QJsonObject result =
        call("login", {{"username", "Q$USER"}, {"password", "Pa$$w$rd"}});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QVERIFY(m_sentToHost.contains(core::EBCDIC::stringToEBCDIC("Q$USER")));
    // Interpolated once into $password, then typed unchanged: "$$" stays.
    QVERIFY(m_sentToHost.contains(core::EBCDIC::stringToEBCDIC("Pa$$w$rd")));
}

void TestScriptDollarEscape::waitForTextMatchesDollarLiterally() {
    auto *screen = m_widget->screenBuffer();
    const QString text = QStringLiteral("COST $5");
    for (int i = 0; i < text.size(); ++i)
        screen->writeChar(10, 2 + i, core::EBCDIC::charToEBCDIC(text[i]));
    const QJsonObject result = call("wait_for_text", {{"text", text}, {"timeout", 1000}});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
}

void TestScriptDollarEscape::waitForTextTimeoutMessageKeepsDollar() {
    const QJsonObject result = call("wait_for_text", {{"text", "Pa$word1"}, {"timeout", 1}});
    QVERIFY(result.value("isError").toBool());
    QVERIFY2(resultText(result).contains("Pa$word1"), qPrintable(resultText(result)));
}

void TestScriptDollarEscape::recordedMacroKeepsDollar() {
    core::Macro macro;
    macro.name = QStringLiteral("dollar");
    for (QChar ch : QString("P$5")) {
        core::MacroStep step;
        step.type = core::MacroStep::KeyPress;
        step.text = QString(ch);
        macro.steps.append(step);
    }
    const QString script = core::macroToScript(macro);
    const QJsonObject result = m_handler->callTool(
        "run_5250script", {{"session_id", kSessionId}, {"script", script}});
    QVERIFY2(!result.value("isError").toBool(), qPrintable(resultText(result)));
    QCOMPARE(fieldText(5, 52), QString("P$5"));
}

QTEST_MAIN(TestScriptDollarEscape)
#include "test_script_dollar_escape.moc"
