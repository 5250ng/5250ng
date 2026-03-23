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

#include "QAgentPanelWidget.h"
#include "agent/agent_script_runner.h"
#include "agent/config.h"
#include "agent/markdown_converter.h"
#include "agent/script_generator.h"
#include "agent/tool_definitions.h"
#include "ui/themes/manager.h"
#include <QEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QTextBlock>
#include <QTextCursor>

namespace ui::widgets {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void appendHtml(QTextBrowser *browser, const QString &html) {
    QTextCursor cursor = browser->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!browser->document()->isEmpty()) {
        cursor.insertBlock();
        QTextBlockFormat spacer;
        spacer.setTopMargin(6);
        spacer.setBottomMargin(6);
        cursor.setBlockFormat(spacer);
    }
    cursor.insertHtml(html);
    browser->setTextCursor(cursor);
    browser->verticalScrollBar()->setValue(
        browser->verticalScrollBar()->maximum());
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

QAgentPanelWidget::QAgentPanelWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Header with title + clear button ---
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(6, 6, 6, 6);
    headerLayout->setSpacing(4);

    m_headerLabel = new QLabel("Agent", this);
    m_headerLabel->setStyleSheet("font-weight: bold;");
    headerLayout->addWidget(m_headerLabel);

    headerLayout->addStretch();

    m_clearButton = new QPushButton("Clear", this);
    m_clearButton->setFixedHeight(22);
    m_clearButton->setStyleSheet("font-size: 11px; padding: 2px 8px;");
    connect(m_clearButton, &QPushButton::clicked, this, [this]() {
        m_chatHistory->clear();
        m_thinkingTimer->stop();
        m_thinkingBlockPosition = -1;
        if (m_scriptRunner && m_scriptRunner->isRunning())
            m_scriptRunner->stop();
        if (m_scriptGenerator)
            m_scriptGenerator->cancel();
        m_pendingToolCall.reset();
        m_toolCallBar->setVisible(false);
        if (m_provider)
            m_provider->clearHistory();
        setInputEnabled(true);
    });
    headerLayout->addWidget(m_clearButton);

    auto *headerWidget = new QWidget(this);
    headerWidget->setLayout(headerLayout);
    headerWidget->setStyleSheet(
        "border-bottom: 1px solid palette(mid);");
    layout->addWidget(headerWidget);

    // --- Chat history ---
    m_chatHistory = new QTextBrowser(this);
    m_chatHistory->setReadOnly(true);
    m_chatHistory->setOpenExternalLinks(true);
    m_chatHistory->setFocusPolicy(Qt::NoFocus);
    m_chatHistory->setTextInteractionFlags(
        Qt::TextBrowserInteraction | Qt::LinksAccessibleByMouse);
    m_chatHistory->setStyleSheet("QTextBrowser { border: none; padding: 4px; }");
    layout->addWidget(m_chatHistory, 1);

    // --- Tool call action bar (hidden by default) ---
    m_toolCallBar = new QWidget(this);
    auto *toolBarLayout = new QHBoxLayout(m_toolCallBar);
    toolBarLayout->setContentsMargins(4, 4, 4, 4);
    toolBarLayout->setSpacing(8);

    m_runScriptButton = new QPushButton("Run Script", m_toolCallBar);
    m_runScriptButton->setStyleSheet(
        "QPushButton { background-color: #2e7d32; color: white; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:disabled { background-color: #555; color: #999; }");
    connect(m_runScriptButton, &QPushButton::clicked, this, &QAgentPanelWidget::onRunScriptClicked);
    toolBarLayout->addWidget(m_runScriptButton);

    m_cancelScriptButton = new QPushButton("Cancel", m_toolCallBar);
    m_cancelScriptButton->setStyleSheet(
        "QPushButton { padding: 6px 16px; }");
    connect(m_cancelScriptButton, &QPushButton::clicked, this, &QAgentPanelWidget::onCancelScriptClicked);
    toolBarLayout->addWidget(m_cancelScriptButton);

    toolBarLayout->addStretch();
    m_toolCallBar->setVisible(false);
    layout->addWidget(m_toolCallBar);

    // --- Input area ---
    auto *inputWrapper = new QWidget(this);
    auto *inputWrapperLayout = new QVBoxLayout(inputWrapper);
    inputWrapperLayout->setContentsMargins(4, 4, 4, 4);
    inputWrapperLayout->setSpacing(0);

    m_inputField = new QPlainTextEdit(inputWrapper);
    m_inputField->setPlaceholderText("Type a message... (Enter to send, Shift+Enter for newline)");
    m_inputField->setMaximumHeight(72);
    m_inputField->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_inputField->installEventFilter(this);
    inputWrapperLayout->addWidget(m_inputField);

    layout->addWidget(inputWrapper);

    // --- Font zoom shortcuts ---
    auto *zoomIn = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
    connect(zoomIn, &QShortcut::activated, this, [this]() { adjustFontSize(1); });
    auto *zoomInAlt = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
    connect(zoomInAlt, &QShortcut::activated, this, [this]() { adjustFontSize(1); });
    auto *zoomOut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
    connect(zoomOut, &QShortcut::activated, this, [this]() { adjustFontSize(-1); });

    // --- Thinking indicator timer ---
    m_thinkingTimer = new QTimer(this);
    m_thinkingTimer->setInterval(500);
    connect(m_thinkingTimer, &QTimer::timeout, this, [this]() {
        m_thinkingDots = (m_thinkingDots % 3) + 1;
        QString dots = QString(".").repeated(m_thinkingDots);

        if (m_thinkingBlockPosition >= 0) {
            QTextCursor cursor(m_chatHistory->document());
            cursor.setPosition(m_thinkingBlockPosition);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            auto &tm = ui::themes::ThemeManager::instance();
            QString labelColor = tm.color("agent.assistant.label", "#50c878");
            cursor.insertHtml(
                QStringLiteral("<i style='color:%1;'>Assistant is thinking%2</i>")
                    .arg(labelColor, dots));
        }
    });
}

// ---------------------------------------------------------------------------
// Show event — auto-focus input field
// ---------------------------------------------------------------------------

void QAgentPanelWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    m_inputField->setFocus();
}

// ---------------------------------------------------------------------------
// Event filter — Enter sends, Shift+Enter inserts newline
// ---------------------------------------------------------------------------

bool QAgentPanelWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_inputField && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
            && !(ke->modifiers() & Qt::ShiftModifier)) {
            submitMessage();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// Submit
// ---------------------------------------------------------------------------

void QAgentPanelWidget::submitMessage() {
    if (!m_inputField->isEnabled()) return;

    QString text = m_inputField->toPlainText().trimmed();
    if (text.isEmpty()) return;

    appendUserMessage(text);
    m_inputField->clear();
    emit messageSubmitted(text);

    if (m_provider && m_provider->isConfigured()) {
        setInputEnabled(false);
        showThinkingIndicator();
        QString systemContext = agent::AgentConfig::instance().systemPrompt();
        if (!m_screenContext.isEmpty()) {
            systemContext += "\n\n--- Current 5250 Screen ---\n" + m_screenContext;
        }
        m_provider->sendMessage(text, systemContext);
    } else {
        appendError("No provider configured. Go to Settings \u2192 Agent to configure one.");
    }
}

// ---------------------------------------------------------------------------
// Provider
// ---------------------------------------------------------------------------

void QAgentPanelWidget::setProvider(agent::Provider *provider) {
    if (m_provider) {
        disconnect(m_provider, nullptr, this, nullptr);
        m_provider->deleteLater();
    }
    m_provider = provider;
    if (m_provider) {
        connect(m_provider, &agent::Provider::responseReceived, this, [this](const QString &text) {
            removeThinkingIndicator();
            appendAssistantMessage(text);
            setInputEnabled(true);
        });
        connect(m_provider, &agent::Provider::responseError, this, [this](const QString &error) {
            removeThinkingIndicator();
            appendError(error);
            setInputEnabled(true);
        });
        connect(m_provider, &agent::Provider::toolCallReceived, this,
                &QAgentPanelWidget::onToolCallReceived);
    }
}

void QAgentPanelWidget::setDisplayWidget(Q5250ScreenWidget *display) {
    m_displayWidget = display;
}

void QAgentPanelWidget::setScreenContext(const QString &screenText) {
    m_screenContext = screenText;
}

// ---------------------------------------------------------------------------
// Message rendering
// ---------------------------------------------------------------------------

void QAgentPanelWidget::appendUserMessage(const QString &text) {
    auto &tm = ui::themes::ThemeManager::instance();
    QString bgColor   = tm.color("agent.user.background", "#2a2d3a");
    QString textColor = tm.color("agent.user.text", "#e0e0e0");

    QString escaped = text.toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));

    // Right-aligned outer table, inner message cell is 80% width
    QString html = QStringLiteral(
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr><td width='17%'></td>"
        "<td width='83%'>"
        "<table width='100%' cellpadding='8' cellspacing='0'>"
        "<tr><td style='background-color:%1; color:%2; text-align:justify;'>%3"
        "</td></tr></table>"
        "</td></tr></table>")
        .arg(bgColor, textColor, escaped);

    appendHtml(m_chatHistory, html);
}

void QAgentPanelWidget::appendAssistantMessage(const QString &text) {
    auto &tm = ui::themes::ThemeManager::instance();
    QString codeBg   = tm.color("agent.code.background", "#1a1a2e");
    QString codeText = tm.color("agent.code.text", "#d4d4d4");

    QString renderedBody = agent::markdownToHtml(text, codeBg, codeText);

    QString html = QStringLiteral("<div style='text-align:justify;'>%1</div>")
                       .arg(renderedBody);

    appendHtml(m_chatHistory, html);
}

void QAgentPanelWidget::appendError(const QString &text) {
    auto &tm = ui::themes::ThemeManager::instance();
    QString errorColor = tm.color("agent.error.text", "#ff6b6b");

    QString html = QStringLiteral("<span style='color:%1;'><b>Error:</b> %2</span>")
                       .arg(errorColor, text.toHtmlEscaped());

    appendHtml(m_chatHistory, html);
}

void QAgentPanelWidget::appendScriptBlock(const QString &script) {
    auto &tm = ui::themes::ThemeManager::instance();
    QString codeBg   = tm.color("agent.code.background", "#1a1a2e");
    QString codeText = tm.color("agent.code.text", "#d4d4d4");

    QString escaped = script.toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));

    QString html = QStringLiteral(
        "<table width='100%' cellpadding='8' cellspacing='0'>"
        "<tr><td style='background-color:%1; color:%2; font-family:monospace;'>"
        "<b>5250script:</b><br/>%3"
        "</td></tr></table>")
        .arg(codeBg, codeText, escaped);

    appendHtml(m_chatHistory, html);
}

// ---------------------------------------------------------------------------
// Tool call handling
// ---------------------------------------------------------------------------

void QAgentPanelWidget::onToolCallReceived(const agent::ToolCall &call) {
    removeThinkingIndicator();

    // Display any text the LLM emitted alongside the tool call
    if (!call.textBefore.isEmpty()) {
        appendAssistantMessage(call.textBefore);
    }

    if (call.name == agent::kToolGenerateScript) {
        onGenerateScriptToolCall(call);
        return;
    }

    // run_5250script: display the script and show Run/Cancel buttons
    QString script = call.scriptText();
    if (script.isEmpty()) {
        appendError("Agent requested a tool call but no script was provided.");
        if (m_provider) {
            agent::ToolResult result;
            result.toolCallId = call.id;
            result.success = false;
            result.output = "No script text provided in tool call arguments.";
            m_provider->sendToolResult(result);
        }
        showThinkingIndicator();
        return;
    }

    appendScriptBlock(script);

    // Store pending state and show action buttons
    m_pendingToolCall = PendingToolCall{call};
    m_runScriptButton->setEnabled(true);
    m_runScriptButton->setText("Run Script");
    m_cancelScriptButton->setEnabled(true);
    m_toolCallBar->setVisible(true);
}

void QAgentPanelWidget::onRunScriptClicked() {
    if (!m_pendingToolCall || !m_displayWidget) return;

    m_runScriptButton->setEnabled(false);
    m_runScriptButton->setText("Running...");
    m_cancelScriptButton->setEnabled(false);

    // Create script runner if needed
    if (!m_scriptRunner) {
        m_scriptRunner = new agent::AgentScriptRunner(m_displayWidget, this);
        connect(m_scriptRunner, &agent::AgentScriptRunner::finished,
                this, &QAgentPanelWidget::onScriptFinished);
    }

    QString script = m_pendingToolCall->call.scriptText();
    m_scriptRunner->runScript(script);
}

void QAgentPanelWidget::onCancelScriptClicked() {
    if (!m_pendingToolCall) return;

    QString toolCallId = m_pendingToolCall->call.id;
    m_pendingToolCall.reset();
    m_toolCallBar->setVisible(false);

    appendError("Script execution cancelled by user.");

    if (m_provider) {
        agent::ToolResult result;
        result.toolCallId = toolCallId;
        result.success = false;
        result.output = "User cancelled script execution.";
        showThinkingIndicator();
        m_provider->sendToolResult(result);
    } else {
        setInputEnabled(true);
    }
}

void QAgentPanelWidget::onScriptFinished(bool success, const QString &log) {
    if (!m_pendingToolCall) return;

    QString toolCallId = m_pendingToolCall->call.id;
    m_pendingToolCall.reset();
    m_toolCallBar->setVisible(false);

    auto &tm = ui::themes::ThemeManager::instance();
    if (success) {
        QString labelColor = tm.color("agent.assistant.label", "#50c878");
        appendHtml(m_chatHistory,
            QStringLiteral("<span style='color:%1;'><b>Script completed.</b></span>")
                .arg(labelColor));
    } else {
        appendError("Script failed: " + log);
    }

    if (m_provider) {
        agent::ToolResult result;
        result.toolCallId = toolCallId;
        result.success = success;
        result.output = log;
        showThinkingIndicator();
        m_provider->sendToolResult(result);
    } else {
        setInputEnabled(true);
    }
}

// ---------------------------------------------------------------------------
// Script generation via subagent
// ---------------------------------------------------------------------------

void QAgentPanelWidget::onGenerateScriptToolCall(const agent::ToolCall &call) {
    QString task = call.taskText();
    if (task.isEmpty()) {
        appendError("Agent requested script generation but no task was provided.");
        if (m_provider) {
            agent::ToolResult result;
            result.toolCallId = call.id;
            result.success = false;
            result.output = "No task description provided.";
            m_provider->sendToolResult(result);
        }
        showThinkingIndicator();
        return;
    }

    appendHtml(m_chatHistory,
        QStringLiteral("<i>Generating script for: %1...</i>")
            .arg(task.toHtmlEscaped()));

    if (!m_scriptGenerator) {
        m_scriptGenerator = new agent::ScriptGeneratorSubagent(this);
    }

    m_scriptGenerator->configure(
        m_provider->id(), m_provider->model(),
        m_provider->apiKey(), m_provider->authMethod());

    QString toolCallId = call.id;

    connect(m_scriptGenerator, &agent::ScriptGeneratorSubagent::scriptGenerated,
            this, [this, toolCallId](const QString &script) {
                onScriptGenerated(toolCallId, script);
            }, Qt::SingleShotConnection);

    connect(m_scriptGenerator, &agent::ScriptGeneratorSubagent::generationError,
            this, [this, toolCallId](const QString &error) {
                onScriptGenerationError(toolCallId, error);
            }, Qt::SingleShotConnection);

    m_scriptGenerator->generate(task, m_screenContext);
}

void QAgentPanelWidget::onScriptGenerated(const QString &toolCallId,
                                          const QString &script) {
    appendScriptBlock(script);

    if (m_provider) {
        agent::ToolResult result;
        result.toolCallId = toolCallId;
        result.success = true;
        result.output = script;
        showThinkingIndicator();
        m_provider->sendToolResult(result);
    } else {
        setInputEnabled(true);
    }
}

void QAgentPanelWidget::onScriptGenerationError(const QString &toolCallId,
                                                const QString &error) {
    appendError("Script generation failed: " + error);

    if (m_provider) {
        agent::ToolResult result;
        result.toolCallId = toolCallId;
        result.success = false;
        result.output = "Script generation failed: " + error;
        showThinkingIndicator();
        m_provider->sendToolResult(result);
    } else {
        setInputEnabled(true);
    }
}

// ---------------------------------------------------------------------------
// Thinking indicator
// ---------------------------------------------------------------------------

void QAgentPanelWidget::showThinkingIndicator() {
    auto &tm = ui::themes::ThemeManager::instance();
    QString labelColor = tm.color("agent.assistant.label", "#50c878");

    QTextCursor cursor = m_chatHistory->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!m_chatHistory->document()->isEmpty())
        cursor.insertBlock();

    m_thinkingBlockPosition = cursor.position();
    cursor.insertHtml(
        QStringLiteral("<i style='color:%1;'>Assistant is thinking.</i>")
            .arg(labelColor));
    m_chatHistory->verticalScrollBar()->setValue(
        m_chatHistory->verticalScrollBar()->maximum());

    m_thinkingDots = 1;
    m_thinkingTimer->start();
}

void QAgentPanelWidget::removeThinkingIndicator() {
    m_thinkingTimer->stop();
    if (m_thinkingBlockPosition < 0) return;

    QTextCursor cursor(m_chatHistory->document());
    cursor.setPosition(m_thinkingBlockPosition);
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    if (!cursor.atStart() && cursor.block().text().isEmpty()) {
        cursor.deletePreviousChar();
    }

    m_thinkingBlockPosition = -1;
}

// ---------------------------------------------------------------------------
// Input state
// ---------------------------------------------------------------------------

void QAgentPanelWidget::setInputEnabled(bool enabled) {
    m_inputField->setEnabled(enabled);
    if (enabled) {
        m_inputField->setFocus();
    }
}

void QAgentPanelWidget::adjustFontSize(int delta) {
    QFont f = font();
    int newSize = qBound(6, f.pointSize() + delta, 32);
    f.setPointSize(newSize);
    setFont(f);
    m_chatHistory->setFont(f);
    m_inputField->setFont(f);
}

} // namespace ui::widgets
