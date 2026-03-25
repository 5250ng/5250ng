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

#include "mcp_log_viewer.h"
#include "agent/config.h"
#include "mcp/McpServer.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QVBoxLayout>

McpLogViewerDialog::McpLogViewerDialog(mcp::McpServer *server, QWidget *parent)
    : ui::widgets::BaseFramelessWindow(parent), m_server(server) {
    setWindowTitle("MCP Server");
    resize(650, 500);

    QVBoxLayout *root = contentLayout();
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto &cfg = agent::AgentConfig::instance();

    // Top bar: enable + port + apply + status
    QHBoxLayout *topBar = new QHBoxLayout();
    m_enableCheck = new QCheckBox("Enable MCP Server", this);
    m_enableCheck->setChecked(cfg.mcpServerEnabled());
    topBar->addWidget(m_enableCheck);

    topBar->addWidget(new QLabel("Port:", this));
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(cfg.mcpServerPort());
    topBar->addWidget(m_portSpin);

    m_applyBtn = new QPushButton("Apply", this);
    connect(m_applyBtn, &QPushButton::clicked, this, &McpLogViewerDialog::onApplyClicked);
    topBar->addWidget(m_applyBtn);

    topBar->addStretch();
    root->addLayout(topBar);

    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("font-style: italic; color: gray;");
    root->addWidget(m_statusLabel);

    // Find bar (hidden by default)
    m_findBar = new QWidget(this);
    m_findBar->setVisible(false);
    QHBoxLayout *findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(0, 0, 0, 0);
    m_findEdit = new QLineEdit(m_findBar);
    m_findEdit->setPlaceholderText("Find...");
    findLayout->addWidget(m_findEdit);
    QPushButton *prevBtn = new QPushButton("<", m_findBar);
    prevBtn->setFixedWidth(30);
    connect(prevBtn, &QPushButton::clicked, this, &McpLogViewerDialog::onFindPrev);
    findLayout->addWidget(prevBtn);
    QPushButton *nextBtn = new QPushButton(">", m_findBar);
    nextBtn->setFixedWidth(30);
    connect(nextBtn, &QPushButton::clicked, this, &McpLogViewerDialog::onFindNext);
    findLayout->addWidget(nextBtn);
    root->addWidget(m_findBar);

    // Log area
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::TypeWriter);
    m_log->setFont(monoFont);
    root->addWidget(m_log, 1);

    // Bottom bar
    QHBoxLayout *bottomBar = new QHBoxLayout();
    bottomBar->addStretch();
    m_clearBtn = new QPushButton("Clear", this);
    connect(m_clearBtn, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
    bottomBar->addWidget(m_clearBtn);
    root->addLayout(bottomBar);

    // Ctrl+F shortcut
    QShortcut *findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        m_findBar->setVisible(!m_findBar->isVisible());
        if (m_findBar->isVisible())
            m_findEdit->setFocus();
    });
    connect(m_findEdit, &QLineEdit::returnPressed, this, &McpLogViewerDialog::onFindNext);

    // Connect to server log signal
    connect(m_server, &mcp::McpServer::requestLog, this, &McpLogViewerDialog::appendLog);
    connect(m_server, &mcp::McpServer::started, this, [this](quint16) { updateStatus(); });
    connect(m_server, &mcp::McpServer::stopped, this, [this]() { updateStatus(); });
    connect(m_server, &mcp::McpServer::error, this, [this](const QString &msg) {
        appendLog("ERROR: " + msg);
        updateStatus();
    });

    updateStatus();
}

bool McpLogViewerDialog::event(QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape && m_findBar->isVisible()) {
            m_findBar->setVisible(false);
            return true;
        }
    }
    return BaseFramelessWindow::event(event);
}

void McpLogViewerDialog::onApplyClicked() {
    auto &cfg = agent::AgentConfig::instance();
    bool enabled = m_enableCheck->isChecked();
    quint16 port = static_cast<quint16>(m_portSpin->value());

    cfg.setMcpServerEnabled(enabled);
    cfg.setMcpServerPort(port);
    cfg.save();

    if (enabled) {
        if (m_server->isRunning())
            m_server->stop();
        m_server->start(port);
        appendLog(QString("Server started on port %1").arg(port));
    } else {
        m_server->stop();
        appendLog("Server stopped");
    }

    updateStatus();
}

void McpLogViewerDialog::updateStatus() {
    if (m_server->isRunning()) {
        m_statusLabel->setText(QString("Listening on localhost:%1").arg(m_server->port()));
        m_statusLabel->setStyleSheet("font-style: italic; color: green;");
    } else {
        m_statusLabel->setText("Stopped");
        m_statusLabel->setStyleSheet("font-style: italic; color: gray;");
    }
}

void McpLogViewerDialog::appendLog(const QString &entry) {
    m_log->appendPlainText(entry);
    // Auto-scroll to bottom
    QScrollBar *sb = m_log->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void McpLogViewerDialog::onFindNext() {
    if (m_findEdit->text().isEmpty()) return;
    if (!m_log->find(m_findEdit->text())) {
        // Wrap around
        QTextCursor cursor = m_log->textCursor();
        cursor.movePosition(QTextCursor::Start);
        m_log->setTextCursor(cursor);
        m_log->find(m_findEdit->text());
    }
}

void McpLogViewerDialog::onFindPrev() {
    if (m_findEdit->text().isEmpty()) return;
    if (!m_log->find(m_findEdit->text(), QTextDocument::FindBackward)) {
        QTextCursor cursor = m_log->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_log->setTextCursor(cursor);
        m_log->find(m_findEdit->text(), QTextDocument::FindBackward);
    }
}
