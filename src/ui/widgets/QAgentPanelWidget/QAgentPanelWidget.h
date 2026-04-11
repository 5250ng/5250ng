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

#pragma once

#include "agent/provider.h"
#include "agent/tool_call.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QMap>
#include <QWidget>
#include <optional>

namespace agent {
class AgentScriptRunner;
class ScriptGeneratorSubagent;
}

namespace ui::widgets {

class Q5250ScreenWidget;

class QAgentPanelWidget : public QWidget {
    Q_OBJECT

  public:
    explicit QAgentPanelWidget(QWidget *parent = nullptr);

    void setProvider(agent::Provider *provider);
    void setDisplayWidget(Q5250ScreenWidget *display);
    void setScreenContext(const QString &screenText);
    void appendUserMessage(const QString &text);
    void appendAssistantMessage(const QString &text);
    void appendError(const QString &text);

  signals:
    void messageSubmitted(const QString &text);

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    void setInputEnabled(bool enabled);
    void submitMessage();
    void adjustFontSize(int delta);
    void showThinkingIndicator();
    void removeThinkingIndicator();
    void onToolCallReceived(const agent::ToolCall &call);
    void onRunScriptClicked();
    void onCancelScriptClicked();
    void onScriptFinished(bool success, const QString &log);
    void onGenerateScriptToolCall(const agent::ToolCall &call);
    void onScriptGenerated(const QString &toolCallId, const QString &script);
    void onScriptGenerationError(const QString &toolCallId, const QString &error);
    void appendScriptBlock(const QString &script);
    void onReadFileToolCall(const agent::ToolCall &call);
    void onWriteFileToolCall(const agent::ToolCall &call);
    void onAcceptFileWriteClicked();
    void onCancelFileWriteClicked();
    void onReadScreenToolCall(const agent::ToolCall &call);
    void onListFilesToolCall(const agent::ToolCall &call);
    void onSendKeysToolCall(const agent::ToolCall &call);
    void onTypeTextToolCall(const agent::ToolCall &call);
    void onSetCursorPositionToolCall(const agent::ToolCall &call);
    void onMoveCursorToolCall(const agent::ToolCall &call);
    void onWaitForTextToolCall(const agent::ToolCall &call);
    void onGetCursorPositionToolCall(const agent::ToolCall &call);
    void onGetScreenSizeToolCall(const agent::ToolCall &call);
    void onGetFieldAtToolCall(const agent::ToolCall &call);
    void onFindTextToolCall(const agent::ToolCall &call);
    void onReadLineToolCall(const agent::ToolCall &call);
    void onReadRegionToolCall(const agent::ToolCall &call);

    QLabel *m_headerLabel;
    QPushButton *m_clearButton;
    QTextBrowser *m_chatHistory;
    QPlainTextEdit *m_inputField;

    // Tool call action bar (visible only during pending tool calls)
    QWidget *m_toolCallBar;
    QPushButton *m_acceptButton;
    QPushButton *m_cancelButton;

    agent::Provider *m_provider = nullptr;
    Q5250ScreenWidget *m_displayWidget = nullptr;
    agent::AgentScriptRunner *m_scriptRunner = nullptr;
    agent::ScriptGeneratorSubagent *m_scriptGenerator = nullptr;
    QString m_screenContext;

    // Pending tool call state
    struct PendingToolCall {
        agent::ToolCall call;
    };
    std::optional<PendingToolCall> m_pendingToolCall;

    QTimer *m_thinkingTimer;
    int m_thinkingDots = 0;
    int m_thinkingBlockPosition = -1;

    // Collapsible subagent output blocks
    struct CollapsibleBlock {
        QString title;       // e.g. "Script generation output"
        QString fullHtml;    // full content HTML
        bool collapsed;
    };
    QMap<int, CollapsibleBlock> m_collapsibleBlocks;
    int m_nextBlockId = 0;
    void appendCollapsibleBlock(const QString &title, const QString &contentHtml);
    void onAnchorClicked(const QUrl &url);
    void replaceCollapsibleBlock(int blockId);
    QString buildCollapsibleHtml(int blockId) const;
};

} // namespace ui::widgets
