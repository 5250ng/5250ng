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

#include "log_viewer.h"
#include "logger/logger.h"
#include <QtUiStyle/StyledMessageBox.h>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QKeySequence>
#include <QStandardPaths>

LogViewerDialog::LogViewerDialog(QWidget *parent)
    : qt_ui_style::BaseFramelessWindow(parent), m_text(new QPlainTextEdit(this)) {
    setWindowTitle("Session Logs");
    resize(800, 600);
    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(8, 8, 8, 8);

    // Find bar (hidden by default)
    m_findBar = new QWidget(this);
    QHBoxLayout *findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(0, 0, 0, 0);
    findLayout->setSpacing(6);
    m_findEdit = new QLineEdit(m_findBar);
    m_findEdit->setPlaceholderText("Find...");
    m_prevBtn = new QPushButton("Prev", m_findBar);
    m_nextBtn = new QPushButton("Next", m_findBar);
    findLayout->addWidget(m_findEdit, 1);
    findLayout->addWidget(m_prevBtn, 0);
    findLayout->addWidget(m_nextBtn, 0);
    m_findBar->setVisible(false);
    content->addWidget(m_findBar, 0);

    m_text->setReadOnly(true);
    m_text->setStyleSheet("QPlainTextEdit { background-color: #000000; color: #ffffff; }");
    content->addWidget(m_text, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);
    m_saveBtn = new QPushButton("Save Diagnostic Log...", this);
    buttonLayout->addWidget(m_saveBtn, 0);
    content->addLayout(buttonLayout, 0);
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() { saveDiagnosticLog(this); });

    // Shortcuts and find connections
    m_shortcutFind = new QShortcut(QKeySequence::Find, this);
    connect(m_shortcutFind, &QShortcut::activated, this, &LogViewerDialog::onFindToggle);
    connect(m_findEdit, &QLineEdit::returnPressed, this, &LogViewerDialog::onFindReturnPressed);
    connect(m_nextBtn, &QPushButton::clicked, this, &LogViewerDialog::onFindNext);
    connect(m_prevBtn, &QPushButton::clicked, this, &LogViewerDialog::onFindPrev);

    // Load existing logs from the in-memory ring buffer
    const QStringList logs = logger::Logger::instance()->recentLogs();
    for (const QString &line : logs) {
        m_text->appendPlainText(line);
    }
    m_text->moveCursor(QTextCursor::End);

    // Subscribe to live updates
    connect(logger::Logger::instance(), &logger::Logger::logMessage,
            this, &LogViewerDialog::onLogMessage);
}

void LogViewerDialog::saveDiagnosticLog(QWidget *parent) {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath();
    }
    const QString defaultPath = dir + QString("/5250ng-log-%1.txt")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString fileName = QFileDialog::getSaveFileName(
        parent, "Save Diagnostic Log", defaultPath, "Text Files (*.txt *.log);;All Files (*)");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)
        || file.write(logger::Logger::instance()->diagnosticReport().toUtf8()) < 0
        || !file.flush()) {
        qt_ui_style::StyledMessageBox::warning(parent, "Save Diagnostic Log",
            QString("Failed to save the log:\n%1").arg(file.errorString()));
        return;
    }
    file.close();
    qt_ui_style::StyledMessageBox::information(parent, "Save Diagnostic Log",
        QString("Log saved to:\n%1\n\nAttach this file to your bug report.").arg(fileName));
}

void LogViewerDialog::onLogMessage(logger::LogLevel /*level*/, const QString &message) {
    m_text->appendPlainText(message);
    m_text->moveCursor(QTextCursor::End);
}

void LogViewerDialog::onFindToggle() {
    const bool willShow = !m_findBar->isVisible();
    m_findBar->setVisible(willShow);
    if (willShow) {
        m_findEdit->setFocus();
        m_findEdit->selectAll();
    } else {
        m_text->setFocus();
    }
}

void LogViewerDialog::onFindReturnPressed() {
    onFindNext();
}

void LogViewerDialog::onFindNext() {
    const QString needle = m_findEdit->text();
    if (needle.isEmpty()) return;
    if (!m_text->find(needle)) {
        QTextCursor cur = m_text->textCursor();
        cur.movePosition(QTextCursor::Start);
        m_text->setTextCursor(cur);
        m_text->find(needle);
    }
}

void LogViewerDialog::onFindPrev() {
    const QString needle = m_findEdit->text();
    if (needle.isEmpty()) return;
    if (!m_text->find(needle, QTextDocument::FindBackward)) {
        QTextCursor cur = m_text->textCursor();
        cur.movePosition(QTextCursor::End);
        m_text->setTextCursor(cur);
        m_text->find(needle, QTextDocument::FindBackward);
    }
}
