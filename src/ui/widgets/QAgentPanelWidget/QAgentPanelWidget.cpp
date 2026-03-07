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
#include <QScrollBar>
#include <QTextCursor>

namespace ui::widgets {

QAgentPanelWidget::QAgentPanelWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_headerLabel = new QLabel("Agent", this);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    m_headerLabel->setStyleSheet(
        "padding: 6px; font-weight: bold; border-bottom: 1px solid palette(mid);");
    layout->addWidget(m_headerLabel);

    m_chatHistory = new QTextEdit(this);
    m_chatHistory->setReadOnly(true);
    m_chatHistory->setStyleSheet("QTextEdit { border: none; padding: 4px; }");
    layout->addWidget(m_chatHistory, 1);

    auto *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(4, 4, 4, 4);
    inputLayout->setSpacing(4);

    m_inputField = new QLineEdit(this);
    m_inputField->setPlaceholderText("Type a message...");
    inputLayout->addWidget(m_inputField);

    m_sendButton = new QPushButton("Send", this);
    inputLayout->addWidget(m_sendButton);

    layout->addLayout(inputLayout);

    auto submitMessage = [this]() {
        // Guard against double-send while waiting for a response
        if (!m_sendButton->isEnabled()) return;

        QString text = m_inputField->text().trimmed();
        if (text.isEmpty()) return;

        appendUserMessage(text);
        m_inputField->clear();
        emit messageSubmitted(text);

        if (m_provider && m_provider->isConfigured()) {
            setInputEnabled(false);
            QString systemContext = agent::AgentConfig::instance().systemPrompt();
            if (!m_screenContext.isEmpty()) {
                systemContext += "\n\n--- Current 5250 Screen ---\n" + m_screenContext;
            }
            m_provider->sendMessage(text, systemContext);
        } else {
            appendError("No provider configured. Go to Settings → Agent to configure one.");
        }
    };

    connect(m_sendButton, &QPushButton::clicked, this, submitMessage);
    connect(m_inputField, &QLineEdit::returnPressed, this, submitMessage);
}

void QAgentPanelWidget::setProvider(agent::Provider *provider) {
    if (m_provider) {
        disconnect(m_provider, nullptr, this, nullptr);
    }
    m_provider = provider;
    if (m_provider) {
        connect(m_provider, &agent::Provider::responseReceived, this, [this](const QString &text) {
            appendAssistantMessage(text);
            setInputEnabled(true);
        });
        connect(m_provider, &agent::Provider::responseError, this, [this](const QString &error) {
            appendError(error);
            setInputEnabled(true);
        });
    }
}

void QAgentPanelWidget::setScreenContext(const QString &screenText) {
    m_screenContext = screenText;
}

static void appendHtml(QTextEdit *edit, const QString &html) {
    QTextCursor cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!edit->document()->isEmpty()) {
        cursor.insertBlock();
    }
    cursor.insertHtml(html);
    edit->verticalScrollBar()->setValue(edit->verticalScrollBar()->maximum());
}

void QAgentPanelWidget::appendUserMessage(const QString &text) {
    appendHtml(m_chatHistory,
        "<b style='color:#4a9eff;'>You:</b> " + text.toHtmlEscaped());
}

void QAgentPanelWidget::appendAssistantMessage(const QString &text) {
    appendHtml(m_chatHistory,
        "<b style='color:#50c878;'>Assistant:</b> " + text.toHtmlEscaped());
}

void QAgentPanelWidget::appendError(const QString &text) {
    appendHtml(m_chatHistory,
        "<span style='color:#ff6b6b;'><b>Error:</b> " + text.toHtmlEscaped() + "</span>");
}

void QAgentPanelWidget::setInputEnabled(bool enabled) {
    m_inputField->setEnabled(enabled);
    m_sendButton->setEnabled(enabled);
    if (enabled) {
        m_inputField->setFocus();
    }
}

} // namespace ui::widgets
