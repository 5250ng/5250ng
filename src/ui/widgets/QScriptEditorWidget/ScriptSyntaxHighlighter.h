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

#include <5250script/script_language.h>
#include <5250script/script_lexer.h>
#include <5250script/script_linter.h>
#include <QHash>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

namespace ui::widgets {

// Colours 5250script source using the 5250script lexer, so colouring always
// matches what the parser accepts. Palette follows the VS Code extension.
// Problems from the linter are drawn as wavy underlines.
class ScriptSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

  public:
    explicit ScriptSyntaxHighlighter(QTextDocument *document);

    // Dark palette (VS Code extension colours) or a higher-contrast light one
    void setDarkPalette(bool dark);
    bool darkPalette() const { return m_dark; }

    // Underline these problems; lines are 1-based like the linter's
    void setDiagnostics(const QVector<core::scripting::Diagnostic> &diagnostics);

    // Format used for a token category, a comment or a metadata comment
    QTextCharFormat categoryFormat(core::scripting::language::TokenCategory category) const;
    QTextCharFormat commentFormat() const { return m_comment; }
    QTextCharFormat metadataFormat() const { return m_metadata; }

  protected:
    void highlightBlock(const QString &text) override;

  private:
    void buildFormats();
    void underline(int start, int length, const QColor &color);

    bool m_dark = true;
    core::scripting::ScriptLexer m_lexer;
    QHash<int, QTextCharFormat> m_formats;  // TokenCategory -> format
    QTextCharFormat m_comment;
    QTextCharFormat m_metadata;
    QTextCharFormat m_interpolated;
    QColor m_errorColor;
    QColor m_warningColor;
    QHash<int, QVector<core::scripting::Diagnostic>> m_diagnostics;  // 0-based block -> problems
};

} // namespace ui::widgets
