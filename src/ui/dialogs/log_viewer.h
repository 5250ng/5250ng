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
#include <QVBoxLayout>

class LogViewerDialog : public qt_ui_style::BaseFramelessWindow {
    Q_OBJECT
  public:
    explicit LogViewerDialog(QWidget *parent = nullptr);
    ~LogViewerDialog() override = default;

  private slots:
    void onLogMessage(logger::LogLevel level, const QString &message);
    void onFindToggle();
    void onFindReturnPressed();
    void onFindNext();
    void onFindPrev();

  private:
    QWidget *m_findBar;
    QLineEdit *m_findEdit;
    QPushButton *m_nextBtn;
    QPushButton *m_prevBtn;
    QShortcut *m_shortcutFind;
    QPlainTextEdit *m_text;
};
