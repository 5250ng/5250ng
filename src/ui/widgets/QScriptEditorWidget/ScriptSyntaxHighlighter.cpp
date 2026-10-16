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

#include "ScriptSyntaxHighlighter.h"
#include <QRegularExpression>
#include <QSet>
#include <QTextDocument>

namespace ui::widgets {

using core::scripting::Diagnostic;
using core::scripting::language::TokenCategory;

ScriptSyntaxHighlighter::ScriptSyntaxHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document) {
    buildFormats();
}

void ScriptSyntaxHighlighter::setDarkPalette(bool dark) {
    if (dark == m_dark)
        return;
    m_dark = dark;
    buildFormats();
    rehighlight();
}

void ScriptSyntaxHighlighter::setDiagnostics(const QVector<Diagnostic> &diagnostics) {
    QHash<int, QVector<Diagnostic>> byBlock;
    for (const Diagnostic &d : diagnostics)
        byBlock[d.line - 1].append(d);
    // Re-colour only the blocks that had or now have problems
    QSet<int> changed;
    for (auto it = m_diagnostics.constBegin(); it != m_diagnostics.constEnd(); ++it)
        changed.insert(it.key());
    for (auto it = byBlock.constBegin(); it != byBlock.constEnd(); ++it)
        changed.insert(it.key());
    m_diagnostics = byBlock;
    for (int blockNumber : changed) {
        const QTextBlock block = document()->findBlockByNumber(blockNumber);
        if (block.isValid())
            rehighlightBlock(block);
    }
}

QTextCharFormat ScriptSyntaxHighlighter::categoryFormat(TokenCategory category) const {
    return m_formats.value(static_cast<int>(category));
}

void ScriptSyntaxHighlighter::buildFormats() {
    auto fmt = [](const char *color, bool italic = false) {
        QTextCharFormat f;
        f.setForeground(QColor(QString::fromLatin1(color)));
        if (italic)
            f.setFontItalic(true);
        return f;
    };
    m_formats.clear();
    auto set = [this](TokenCategory c, const QTextCharFormat &f) {
        m_formats.insert(static_cast<int>(c), f);
    };
    if (m_dark) {
        // Same colours as the 5250script VS Code extension; strings,
        // comments and numbers use VS Code's Dark+ defaults
        set(TokenCategory::Command, fmt("#4CAF50"));
        set(TokenCategory::Control, fmt("#FF9800"));
        set(TokenCategory::AidKey, fmt("#F44336"));
        set(TokenCategory::LocalKey, fmt("#E91E63"));
        set(TokenCategory::CharKey, fmt("#9C27B0"));
        set(TokenCategory::SubKeyword, fmt("#00BCD4"));
        set(TokenCategory::Operator, fmt("#607D8B"));
        set(TokenCategory::Variable, fmt("#F44336"));
        set(TokenCategory::Function, fmt("#DCDCAA"));
        set(TokenCategory::Label, fmt("#4EC9B0"));
        set(TokenCategory::String, fmt("#CE9178"));
        set(TokenCategory::Number, fmt("#B5CEA8"));
        m_comment = fmt("#6A9955", true);
        m_metadata = fmt("#569CD6", true);
        m_interpolated = fmt("#F44336");
        m_errorColor = QColor("#F14C4C");
        m_warningColor = QColor("#CCA700");
    } else {
        set(TokenCategory::Command, fmt("#2E7D32"));
        set(TokenCategory::Control, fmt("#E65100"));
        set(TokenCategory::AidKey, fmt("#C62828"));
        set(TokenCategory::LocalKey, fmt("#AD1457"));
        set(TokenCategory::CharKey, fmt("#6A1B9A"));
        set(TokenCategory::SubKeyword, fmt("#00838F"));
        set(TokenCategory::Operator, fmt("#455A64"));
        set(TokenCategory::Variable, fmt("#C62828"));
        set(TokenCategory::Function, fmt("#795E26"));
        set(TokenCategory::Label, fmt("#267F99"));
        set(TokenCategory::String, fmt("#A31515"));
        set(TokenCategory::Number, fmt("#098658"));
        m_comment = fmt("#008000", true);
        m_metadata = fmt("#0000FF", true);
        m_interpolated = fmt("#C62828");
        m_errorColor = QColor("#E51400");
        m_warningColor = QColor("#BF8803");
    }
}

void ScriptSyntaxHighlighter::highlightBlock(const QString &text) {
    namespace lang = core::scripting::language;

    const int commentAt = lang::commentStart(text);
    const QString code = commentAt >= 0 ? text.left(commentAt) : text;
    const core::scripting::TokenLine tokens = m_lexer.tokenizeLine(code, currentBlock().blockNumber() + 1);
    const QVector<TokenCategory> categories = lang::classifyLine(tokens, code);

    static const QRegularExpression interpolated(QStringLiteral("\\$[A-Za-z_]\\w*"));
    for (int i = 0; i < tokens.size(); ++i) {
        const auto &tok = tokens[i];
        const auto it = m_formats.constFind(static_cast<int>(categories[i]));
        if (it != m_formats.constEnd())
            setFormat(tok.column - 1, tok.length, it.value());
        // $NAME inside a quoted string is interpolated at run time
        if (categories[i] == TokenCategory::String) {
            const QString source = text.mid(tok.column - 1, tok.length);
            auto m = interpolated.globalMatch(source);
            while (m.hasNext()) {
                const auto match = m.next();
                setFormat(tok.column - 1 + static_cast<int>(match.capturedStart()),
                          static_cast<int>(match.capturedLength()), m_interpolated);
            }
        }
    }

    if (commentAt >= 0)
        setFormat(commentAt, text.length() - commentAt,
                  lang::isMetadataComment(text) ? m_metadata : m_comment);

    for (const Diagnostic &d : m_diagnostics.value(currentBlock().blockNumber())) {
        const QColor color = d.severity == Diagnostic::Severity::Error ? m_errorColor : m_warningColor;
        // Zero-length problems (empty lines) underline the whole line
        underline(d.column - 1, d.length > 0 ? d.length : text.length(), color);
    }
}

void ScriptSyntaxHighlighter::underline(int start, int length, const QColor &color) {
    // Keep each character's colour; only add the underline
    for (int pos = qMax(0, start); pos < start + length && pos < currentBlock().length(); ++pos) {
        QTextCharFormat f = format(pos);
        f.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        f.setUnderlineColor(color);
        setFormat(pos, 1, f);
    }
}

} // namespace ui::widgets
