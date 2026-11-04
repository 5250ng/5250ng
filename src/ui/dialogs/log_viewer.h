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

#include "logger/logger.h"
#include <QtUiStyle/BaseFramelessWindow.h>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

class LogViewerDialog : public qt_ui_style::BaseFramelessWindow {
    Q_OBJECT
  public:
    // The application log: the logger's in-memory buffer, then live lines
    explicit LogViewerDialog(QWidget *parent = nullptr);
    // A session log file: its last kMaxInitialBytes, then whatever is
    // appended to it while the window is open
    LogViewerDialog(const QString &filePath, const QString &title, QWidget *parent = nullptr);
    ~LogViewerDialog() override = default;

    static constexpr qint64 kMaxInitialBytes = 2 * 1024 * 1024;
    static constexpr int kMaxFileLines = 20000;
    static constexpr int kFollowIntervalMs = 500;

    QString filePath() const { return m_filePath; }

    /**
     * Ask for a destination (Desktop by default) and write
     * Logger::diagnosticReport() to it. Reports failures in a message box.
     */
    static void saveDiagnosticLog(QWidget *parent);

  private slots:
    void onLogLine(const QString &line);
    void readNewFileData();
    void onFindToggle();
    void onFindReturnPressed();
    void onFindNext();
    void onFindPrev();

  private:
    void buildUi();
    // Append text, keeping the view at the bottom only if it already was
    void appendFollowing(const QString &text);

    QWidget *m_findBar;
    QLineEdit *m_findEdit;
    QPushButton *m_nextBtn;
    QPushButton *m_prevBtn;
    QShortcut *m_shortcutFind;
    QPushButton *m_saveBtn;
    QPlainTextEdit *m_text;

    // Session log file mode
    QString m_filePath;
    qint64 m_fileOffset = 0;
    QByteArray m_partialLine;   // bytes after the last newline read so far
    bool m_skipToNewline = false; // the first read starts mid-line
};
