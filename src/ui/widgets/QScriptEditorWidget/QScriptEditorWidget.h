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

#include "ScriptCodeEditor.h"
#include <QWidget>

class QLabel;
class QTreeWidget;

namespace ui::widgets {

// Script editor with a Problems list under it. Double-clicking a problem
// moves the cursor to it.
class QScriptEditorWidget : public QWidget {
    Q_OBJECT

  public:
    explicit QScriptEditorWidget(QWidget *parent = nullptr);

    ScriptCodeEditor *editor() const { return m_editor; }
    QTreeWidget *problemsList() const { return m_problems; }

    QString text() const { return m_editor->toPlainText(); }
    // Replace the whole script (not undoable) and lint it right away
    void setText(const QString &text);

    int errorCount() const { return m_errorCount; }
    int warningCount() const { return m_warningCount; }

  signals:
    void problemsChanged(int errors, int warnings);

  private:
    void showDiagnostics(const QVector<core::scripting::Diagnostic> &diagnostics);

    ScriptCodeEditor *m_editor = nullptr;
    QLabel *m_problemsHeader = nullptr;
    QTreeWidget *m_problems = nullptr;
    int m_errorCount = 0;
    int m_warningCount = 0;
};

} // namespace ui::widgets
