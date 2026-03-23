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
#include <QWidget>
#include <optional>

namespace agent {
class AgentScriptRunner;
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
    void appendScriptBlock(const QString &script);

    QLabel *m_headerLabel;
    QPushButton *m_clearButton;
    QTextBrowser *m_chatHistory;
    QPlainTextEdit *m_inputField;

    // Tool call action bar (visible only during pending tool calls)
    QWidget *m_toolCallBar;
    QPushButton *m_runScriptButton;
    QPushButton *m_cancelScriptButton;

    agent::Provider *m_provider = nullptr;
    Q5250ScreenWidget *m_displayWidget = nullptr;
    agent::AgentScriptRunner *m_scriptRunner = nullptr;
    QString m_screenContext;

    // Pending tool call state
    struct PendingToolCall {
        agent::ToolCall call;
    };
    std::optional<PendingToolCall> m_pendingToolCall;

    QTimer *m_thinkingTimer;
    int m_thinkingDots = 0;
    int m_thinkingBlockPosition = -1;
};

} // namespace ui::widgets
