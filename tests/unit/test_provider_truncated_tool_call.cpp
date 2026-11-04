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

#include "agent/providers/anthropic_provider.h"
#include "agent/providers/openai_provider.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest/QtTest>

// Exposes the conversation history the next request would send
template <typename P> class Probe : public P {
  public:
    QJsonArray history() const { return this->m_conversationHistory; }
};

static QByteArray json(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }

// Regression tests for issue #259: a response holding a tool call that
// stopped for another reason (max_tokens) stored the tool call in history
// without a result, so the API rejected every later request.
class TestProviderTruncatedToolCall : public QObject {
    Q_OBJECT

  private slots:
    void anthropicCutOffToolUseIsNotKept() {
        Probe<agent::AnthropicProvider> p;
        QSignalSpy tool(&p, &agent::Provider::toolCallReceived);
        QSignalSpy error(&p, &agent::Provider::responseError);
        QSignalSpy text(&p, &agent::Provider::responseReceived);
        p.processResponseBody(json({
            {"stop_reason", "max_tokens"},
            {"content", QJsonArray{
                QJsonObject{{"type", "text"}, {"text", "Here is the script"}},
                QJsonObject{{"type", "tool_use"}, {"id", "toolu_1"}, {"name", "run_5250script"},
                            {"input", QJsonObject{{"script", "TYPE \"half"}}}}}}}));
        QCOMPARE(tool.count(), 0);
        QCOMPARE(error.count(), 1);
        QVERIFY(error.at(0).at(0).toString().contains("cut off"));
        QCOMPARE(text.count(), 1);
        const QJsonArray h = p.history();
        QCOMPARE(h.size(), 1);
        for (const QJsonValue &block : h.last().toObject()["content"].toArray())
            QVERIFY(block.toObject()["type"].toString() != "tool_use");
    }

    void anthropicCompleteToolUseStillRuns() {
        Probe<agent::AnthropicProvider> p;
        QSignalSpy tool(&p, &agent::Provider::toolCallReceived);
        p.processResponseBody(json({
            {"stop_reason", "tool_use"},
            {"content", QJsonArray{
                QJsonObject{{"type", "tool_use"}, {"id", "toolu_1"}, {"name", "run_5250script"},
                            {"input", QJsonObject{{"script", "PRESS ENTER"}}}}}}}));
        QCOMPARE(tool.count(), 1);
    }

    void openAiCutOffToolCallIsNotKept() {
        Probe<agent::OpenAiProvider> p;
        QSignalSpy tool(&p, &agent::Provider::toolCallReceived);
        QSignalSpy error(&p, &agent::Provider::responseError);
        p.processResponseBody(json({
            {"choices", QJsonArray{QJsonObject{
                {"finish_reason", "length"},
                {"message", QJsonObject{
                    {"role", "assistant"},
                    {"content", QJsonValue::Null},
                    {"tool_calls", QJsonArray{QJsonObject{
                        {"id", "call_1"}, {"type", "function"},
                        {"function", QJsonObject{{"name", "run_5250script"}, {"arguments", "{\"scr"}}}}}}}}}}}}));
        QCOMPARE(tool.count(), 0);
        QCOMPARE(error.count(), 1);
        QCOMPARE(p.history().size(), 0); // nothing valid to keep
    }

    void anthropicOnlyCutOffToolUseKeepsNoTurn() {
        Probe<agent::AnthropicProvider> p;
        QSignalSpy error(&p, &agent::Provider::responseError);
        QSignalSpy text(&p, &agent::Provider::responseReceived);
        p.processResponseBody(json({
            {"stop_reason", "max_tokens"},
            {"content", QJsonArray{
                QJsonObject{{"type", "tool_use"}, {"id", "toolu_1"}, {"name", "run_5250script"},
                            {"input", QJsonObject{}}}}}}));
        QCOMPARE(error.count(), 1);
        QCOMPARE(text.count(), 0);
        QCOMPARE(p.history().size(), 0);
    }

    void openAiCutOffKeepsTextWithoutToolCalls() {
        Probe<agent::OpenAiProvider> p;
        QSignalSpy text(&p, &agent::Provider::responseReceived);
        p.processResponseBody(json({
            {"choices", QJsonArray{QJsonObject{
                {"finish_reason", "length"},
                {"message", QJsonObject{
                    {"role", "assistant"},
                    {"content", "Here is the script"},
                    {"tool_calls", QJsonArray{QJsonObject{
                        {"id", "call_1"}, {"type", "function"},
                        {"function", QJsonObject{{"name", "run_5250script"}, {"arguments", "{\"scr"}}}}}}}}}}}}));
        QCOMPARE(text.count(), 1);
        QCOMPARE(p.history().size(), 1);
        QCOMPARE(p.history().at(0).toObject()["content"].toString(), QStringLiteral("Here is the script"));
        QVERIFY(!p.history().at(0).toObject().contains("tool_calls"));
    }

    void openAiEmptyReplyIsNotKept() {
        Probe<agent::OpenAiProvider> p;
        p.processResponseBody(json({
            {"choices", QJsonArray{QJsonObject{
                {"finish_reason", "content_filter"},
                {"message", QJsonObject{{"role", "assistant"}, {"content", QJsonValue::Null}}}}}}}));
        QCOMPARE(p.history().size(), 0);
    }

    void openAiCompleteToolCallStillRuns() {
        Probe<agent::OpenAiProvider> p;
        QSignalSpy tool(&p, &agent::Provider::toolCallReceived);
        p.processResponseBody(json({
            {"choices", QJsonArray{QJsonObject{
                {"finish_reason", "tool_calls"},
                {"message", QJsonObject{
                    {"role", "assistant"},
                    {"tool_calls", QJsonArray{QJsonObject{
                        {"id", "call_1"}, {"type", "function"},
                        {"function", QJsonObject{{"name", "run_5250script"}, {"arguments", "{}"}}}}}}}}}}}}));
        QCOMPARE(tool.count(), 1);
    }
};

QTEST_MAIN(TestProviderTruncatedToolCall)
#include "test_provider_truncated_tool_call.moc"
