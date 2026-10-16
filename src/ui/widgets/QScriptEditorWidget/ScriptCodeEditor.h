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

#include "ScriptSyntaxHighlighter.h"
#include <5250script/script_linter.h>
#include <QPlainTextEdit>
#include <QVector>

class QCompleter;
class QStandardItemModel;
class QTimer;

namespace ui::widgets {

// Plain-text editor for 5250script: syntax colouring, line numbers with
// problem markers, context-aware completion, live linting, auto-indent,
// Tab/Shift+Tab block indent, Ctrl+/ comment toggle and formatting.
class ScriptCodeEditor : public QPlainTextEdit {
    Q_OBJECT

  public:
    explicit ScriptCodeEditor(QWidget *parent = nullptr);

    ScriptSyntaxHighlighter *highlighter() const { return m_highlighter; }
    QCompleter *completer() const { return m_completer; }
    const QVector<core::scripting::Diagnostic> &diagnostics() const { return m_diagnostics; }

    // Move the cursor to a 1-based line and column and show it
    void goToLine(int line, int column = 1);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    QString lineNumberAreaToolTip(int y) const;

  public slots:
    // Re-indent the whole script (undoable as one step)
    void formatDocument();
    // Comment or uncomment the selected lines (or the current line)
    void toggleComment();
    // Open the completion list at the cursor, even after a space
    void triggerCompletion();
    // Lint immediately instead of waiting for typing to pause
    void lintNow();

  signals:
    void diagnosticsChanged(const QVector<core::scripting::Diagnostic> &diagnostics);

  protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;

  private:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();
    void updateCompletion(bool explicitRequest);
    void insertCompletion(const QString &insertText);
    void insertNewlineWithIndent();
    void indentSelection(bool increase);
    void reindentClosingKeyword();
    void syncPaletteMode();
    QString diagnosticsText(int blockNumber, int column) const;

    QWidget *m_lineNumberArea = nullptr;
    ScriptSyntaxHighlighter *m_highlighter = nullptr;
    QCompleter *m_completer = nullptr;
    QStandardItemModel *m_completionModel = nullptr;
    int m_completionStart = 0;  // document position the completion replaces from
    QTimer *m_lintTimer = nullptr;
    QVector<core::scripting::Diagnostic> m_diagnostics;
};

} // namespace ui::widgets
