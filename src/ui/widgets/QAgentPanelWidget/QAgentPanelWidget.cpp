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
#include "agent/config.h"
#include "agent/markdown_converter.h"
#include "ui/themes/manager.h"
#include <QEvent>
#include <QKeyEvent>
#include <QScrollBar>
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
    m_chatHistory->setStyleSheet("QTextBrowser { border: none; padding: 4px; }");
    layout->addWidget(m_chatHistory, 1);

    // --- Input area ---
    auto *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(4, 4, 4, 4);
    inputLayout->setSpacing(4);

    m_inputField = new QPlainTextEdit(this);
    m_inputField->setPlaceholderText("Type a message...");
    m_inputField->setMaximumHeight(72);
    m_inputField->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_inputField->installEventFilter(this);
    inputLayout->addWidget(m_inputField);

    m_sendButton = new QPushButton("Send", this);
    m_sendButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    inputLayout->addWidget(m_sendButton);

    layout->addLayout(inputLayout);

    connect(m_sendButton, &QPushButton::clicked, this, &QAgentPanelWidget::submitMessage);

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
            auto &tm = ui::themes::ThemeManager::instance();
            QString labelColor = tm.color("agent.assistant.label", "#50c878");
            cursor.insertHtml(
                QStringLiteral("<i style='color:%1;'>Assistant is thinking%2</i>")
                    .arg(labelColor, dots));
        }
    });
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
    if (!m_sendButton->isEnabled()) return;

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
    }
}

void QAgentPanelWidget::setScreenContext(const QString &screenText) {
    m_screenContext = screenText;
}

// ---------------------------------------------------------------------------
// Message rendering
// ---------------------------------------------------------------------------

void QAgentPanelWidget::appendUserMessage(const QString &text) {
    auto &tm = ui::themes::ThemeManager::instance();
    QString bgColor    = tm.color("agent.user.background", "#2a2d3a");
    QString textColor  = tm.color("agent.user.text", "#e0e0e0");
    QString labelColor = tm.color("agent.user.label", "#4a9eff");

    QString escaped = text.toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));

    QString html = QStringLiteral(
        "<table width='100%' cellpadding='8' cellspacing='0'>"
        "<tr><td style='background-color:%1; color:%2;'>"
        "<b style='color:%3;'>You</b><br/>%4"
        "</td></tr></table>")
        .arg(bgColor, textColor, labelColor, escaped);

    appendHtml(m_chatHistory, html);
}

void QAgentPanelWidget::appendAssistantMessage(const QString &text) {
    auto &tm = ui::themes::ThemeManager::instance();
    QString labelColor = tm.color("agent.assistant.label", "#50c878");
    QString codeBg     = tm.color("agent.code.background", "#1a1a2e");
    QString codeText   = tm.color("agent.code.text", "#d4d4d4");

    QString renderedBody = agent::markdownToHtml(text, codeBg, codeText);

    QString html = QStringLiteral("<b style='color:%1;'>Assistant</b><br/>%2")
                       .arg(labelColor, renderedBody);

    appendHtml(m_chatHistory, html);
}

void QAgentPanelWidget::appendError(const QString &text) {
    auto &tm = ui::themes::ThemeManager::instance();
    QString errorColor = tm.color("agent.error.text", "#ff6b6b");

    QString html = QStringLiteral("<span style='color:%1;'><b>Error:</b> %2</span>")
                       .arg(errorColor, text.toHtmlEscaped());

    appendHtml(m_chatHistory, html);
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
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    // Also select the block itself so the whole line is removed
    cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    // Select from start of block to end
    cursor.setPosition(m_thinkingBlockPosition);
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    // Remove the empty block
    if (cursor.atStart() && cursor.block().text().isEmpty()) {
        // nothing extra to do
    } else {
        cursor.deletePreviousChar(); // remove the block separator
    }

    m_thinkingBlockPosition = -1;
}

// ---------------------------------------------------------------------------
// Input state
// ---------------------------------------------------------------------------

void QAgentPanelWidget::setInputEnabled(bool enabled) {
    m_inputField->setEnabled(enabled);
    m_sendButton->setEnabled(enabled);
    if (enabled) {
        m_inputField->setFocus();
    }
}

} // namespace ui::widgets
