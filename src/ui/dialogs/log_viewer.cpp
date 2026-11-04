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
#include <QLabel>
#include <QScrollBar>
#include <QStandardPaths>

LogViewerDialog::LogViewerDialog(QWidget *parent)
    : qt_ui_style::BaseFramelessWindow(parent), m_text(new QPlainTextEdit(this)) {
    // Help > View Logs; Session > View Session Logs shows the tab's own log
    setWindowTitle("Application Log");
    buildUi();
    // Keep as much as the logger's own buffer, not every line ever logged
    m_text->setMaximumBlockCount(logger::Logger::maxRecentLogs());

    // Load existing logs from the in-memory ring buffer
    const QStringList logs = logger::Logger::instance()->recentLogs();
    for (const QString &line : logs) {
        m_text->appendPlainText(line);
    }
    m_text->moveCursor(QTextCursor::End);

    // Subscribe to live updates, formatted like the lines loaded above
    connect(logger::Logger::instance(), &logger::Logger::logLineWritten,
            this, &LogViewerDialog::onLogLine);
}

LogViewerDialog::LogViewerDialog(const QString &filePath, const QString &title, QWidget *parent)
    : qt_ui_style::BaseFramelessWindow(parent), m_text(new QPlainTextEdit(this)),
      m_filePath(filePath) {
    setWindowTitle(title);
    buildUi();
    m_text->setMaximumBlockCount(kMaxFileLines);
    // The diagnostic report is the application log, not this file
    m_saveBtn->setVisible(false);
    auto *pathLabel = new QLabel(QDir::toNativeSeparators(filePath), this);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contentLayout()->insertWidget(0, pathLabel, 0);

    QFile file(filePath);
    if (file.size() > kMaxInitialBytes && file.open(QIODevice::ReadOnly)) {
        m_fileOffset = file.size() - kMaxInitialBytes;
        // Start at the next line, unless the offset is already at one
        char before = 0;
        m_skipToNewline = !(file.seek(m_fileOffset - 1) && file.getChar(&before) && before == '\n');
        file.close();
    }
    readNewFileData();
    m_text->moveCursor(QTextCursor::End);

    // Poll rather than watch: QFileSystemWatcher does not report appends on
    // every platform, and the file may not exist yet or be recreated
    auto *timer = new QTimer(this);
    timer->setInterval(kFollowIntervalMs);
    connect(timer, &QTimer::timeout, this, &LogViewerDialog::readNewFileData);
    timer->start();
}

void LogViewerDialog::buildUi() {
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

void LogViewerDialog::onLogLine(const QString &line) {
    appendFollowing(line);
}

void LogViewerDialog::appendFollowing(const QString &text) {
    // Follow new lines only while the view is at the bottom; a user reading
    // or searching further up keeps their place and selection
    QScrollBar *bar = m_text->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum();
    const int value = bar->value();
    const QTextCursor cursor = m_text->textCursor();
    m_text->appendPlainText(text);
    if (atBottom) {
        m_text->moveCursor(QTextCursor::End);
    } else {
        m_text->setTextCursor(cursor);
        bar->setValue(value);
    }
}

void LogViewerDialog::readNewFileData() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    if (file.size() < m_fileOffset) {
        // Truncated or replaced: start over from its beginning
        m_text->clear();
        m_fileOffset = 0;
        m_partialLine.clear();
        m_skipToNewline = false;
    }
    if (!file.seek(m_fileOffset))
        return;
    const QByteArray data = file.read(kMaxInitialBytes);
    if (data.isEmpty())
        return;
    m_fileOffset += data.size();
    m_partialLine += data;
    if (m_skipToNewline) {
        const int newline = m_partialLine.indexOf('\n');
        if (newline < 0) {
            m_partialLine.clear();
            return;
        }
        m_partialLine.remove(0, newline + 1);
        m_skipToNewline = false;
    }
    // Show complete lines only; the rest waits for its newline
    const int lastNewline = m_partialLine.lastIndexOf('\n');
    if (lastNewline < 0)
        return;
    QByteArray complete = m_partialLine.left(lastNewline);
    m_partialLine.remove(0, lastNewline + 1);
    // Text-mode files end lines with CRLF on Windows
    if (complete.endsWith('\r'))
        complete.chop(1);
    complete.replace("\r\n", "\n");
    // Only the last kMaxFileLines can stay in the view; inserting more
    // only to trim them again is slow for very short lines
    int cut = complete.size();
    for (int n = 0; n < kMaxFileLines && cut > 0; ++n)
        cut = complete.lastIndexOf('\n', cut - 1);
    if (cut > 0)
        complete.remove(0, cut + 1);
    appendFollowing(QString::fromUtf8(complete));
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
