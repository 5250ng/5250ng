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

#include "ScriptCodeEditor.h"
#include <5250script/script_completion.h>
#include <5250script/script_formatter.h>
#include <5250script/script_language.h>
#include <QAbstractItemView>
#include <QCompleter>
#include <QFontDatabase>
#include <QHelpEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTextBlock>
#include <QTimer>
#include <QToolTip>

namespace ui::widgets {

using core::scripting::Diagnostic;

namespace {

constexpr int kIndentWidth = 4;
constexpr int kLintDelayMs = 300;
constexpr int kMarkerWidth = 12;
const QString kIndentUnit = QStringLiteral("    ");

QString leadingWhitespace(const QString &text) {
    int i = 0;
    while (i < text.length() && text[i].isSpace())
        ++i;
    return text.left(i);
}

// Upper-cased first word of the code on a line
QString firstKeyword(const QString &text) {
    const int comment = core::scripting::language::commentStart(text);
    const QString code = (comment >= 0 ? text.left(comment) : text).trimmed();
    return code.section(QRegularExpression(QStringLiteral("[\\s(]")), 0, 0).toUpper();
}

bool opensBlock(const QString &keyword) {
    return keyword == "IF" || keyword == "WHILE" || keyword == "REPEAT" || keyword == "DEF"
           || keyword == "ELSE";
}

bool closesBlock(const QString &keyword) {
    return keyword == "ENDIF" || keyword == "ENDWHILE" || keyword == "ENDREPEAT"
           || keyword == "ENDDEF" || keyword == "ELSE";
}

class LineNumberArea : public QWidget {
  public:
    explicit LineNumberArea(ScriptCodeEditor *editor) : QWidget(editor), m_editor(editor) {}
    QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }

  protected:
    void paintEvent(QPaintEvent *event) override { m_editor->lineNumberAreaPaintEvent(event); }
    bool event(QEvent *event) override {
        if (event->type() == QEvent::ToolTip) {
            auto *help = static_cast<QHelpEvent *>(event);
            const QString text = m_editor->lineNumberAreaToolTip(help->pos().y());
            if (text.isEmpty())
                QToolTip::hideText();
            else
                QToolTip::showText(help->globalPos(), text, this);
            return true;
        }
        return QWidget::event(event);
    }

  private:
    ScriptCodeEditor *m_editor;
};

} // namespace

ScriptCodeEditor::ScriptCodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * kIndentWidth);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_lineNumberArea = new LineNumberArea(this);
    m_highlighter = new ScriptSyntaxHighlighter(document());
    syncPaletteMode();

    m_completionModel = new QStandardItemModel(this);
    m_completer = new QCompleter(this);
    m_completer->setWidget(this);
    m_completer->setModel(m_completionModel);
    // Filtering is done by core::scripting::complete()
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setMaxVisibleItems(12);
    connect(m_completer, QOverload<const QModelIndex &>::of(&QCompleter::activated), this,
            [this](const QModelIndex &index) {
                insertCompletion(index.data(Qt::UserRole).toString());
            });

    m_lintTimer = new QTimer(this);
    m_lintTimer->setSingleShot(true);
    m_lintTimer->setInterval(kLintDelayMs);
    connect(m_lintTimer, &QTimer::timeout, this, &ScriptCodeEditor::lintNow);
    connect(this, &QPlainTextEdit::textChanged, m_lintTimer, qOverload<>(&QTimer::start));

    connect(this, &QPlainTextEdit::blockCountChanged, this, [this]() { updateLineNumberAreaWidth(); });
    connect(this, &QPlainTextEdit::updateRequest, this, &ScriptCodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &ScriptCodeEditor::highlightCurrentLine);
    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

void ScriptCodeEditor::goToLine(int line, int column) {
    const QTextBlock block = document()->findBlockByNumber(qMax(0, line - 1));
    if (!block.isValid())
        return;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                        qBound(0, column - 1, block.length() - 1));
    setTextCursor(cursor);
    centerCursor();
    setFocus();
}

// ---------------------------------------------------------------------------
// Line numbers and problem markers
// ---------------------------------------------------------------------------

int ScriptCodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    for (int max = qMax(1, blockCount()); max >= 10; max /= 10)
        ++digits;
    digits = qMax(digits, 3);
    return kMarkerWidth + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 8;
}

void ScriptCodeEditor::updateLineNumberAreaWidth() {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void ScriptCodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void ScriptCodeEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void ScriptCodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), palette().alternateBase());

    // Worst problem on each line: 2 = error, 1 = warning
    QHash<int, int> severity;
    for (const Diagnostic &d : m_diagnostics) {
        const int level = d.severity == Diagnostic::Severity::Error ? 2 : 1;
        severity[d.line - 1] = qMax(severity.value(d.line - 1), level);
    }
    const QColor errorColor = m_highlighter->darkPalette() ? QColor("#F14C4C") : QColor("#E51400");
    const QColor warningColor = m_highlighter->darkPalette() ? QColor("#CCA700") : QColor("#BF8803");
    QColor numberColor = palette().color(QPalette::PlaceholderText);
    const int current = textCursor().blockNumber();

    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    const int lineHeight = fontMetrics().height();
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const int number = block.blockNumber();
            if (const int level = severity.value(number)) {
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(Qt::NoPen);
                painter.setBrush(level == 2 ? errorColor : warningColor);
                const int size = qMin(8, lineHeight - 4);
                painter.drawEllipse(QRect(2, top + (lineHeight - size) / 2, size, size));
            }
            painter.setPen(number == current ? palette().color(QPalette::Text) : numberColor);
            painter.drawText(0, top, m_lineNumberArea->width() - 4, lineHeight,
                             Qt::AlignRight, QString::number(number + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
}

QString ScriptCodeEditor::lineNumberAreaToolTip(int y) const {
    const QTextCursor cursor = cursorForPosition(QPoint(0, y));
    return diagnosticsText(cursor.blockNumber(), -1);
}

// Messages for a block; column -1 means every problem on the line
QString ScriptCodeEditor::diagnosticsText(int blockNumber, int column) const {
    QStringList lines;
    for (const Diagnostic &d : m_diagnostics) {
        if (d.line - 1 != blockNumber)
            continue;
        if (column >= 0 && d.length > 0 && (column < d.column - 1 || column >= d.column - 1 + d.length))
            continue;
        lines << QString("%1: %2")
                     .arg(d.severity == Diagnostic::Severity::Error ? tr("Error") : tr("Warning"),
                          d.message);
    }
    return lines.join('\n');
}

bool ScriptCodeEditor::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const QTextCursor cursor = cursorForPosition(viewport()->mapFromParent(help->pos()));
        const QString text = diagnosticsText(cursor.blockNumber(), cursor.positionInBlock());
        if (text.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(help->globalPos(), text, this);
        return true;
    }
    return QPlainTextEdit::event(event);
}

void ScriptCodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> selections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor color = palette().color(QPalette::Highlight);
        color.setAlpha(40);
        selection.format.setBackground(color);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }
    setExtraSelections(selections);
    m_lineNumberArea->update();
}

void ScriptCodeEditor::changeEvent(QEvent *event) {
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        syncPaletteMode();
}

void ScriptCodeEditor::syncPaletteMode() {
    m_highlighter->setDarkPalette(palette().color(QPalette::Base).lightness() < 128);
}

// ---------------------------------------------------------------------------
// Linting
// ---------------------------------------------------------------------------

void ScriptCodeEditor::lintNow() {
    m_lintTimer->stop();
    m_diagnostics = core::scripting::lint(toPlainText());
    m_highlighter->setDiagnostics(m_diagnostics);
    m_lineNumberArea->update();
    emit diagnosticsChanged(m_diagnostics);
}

// ---------------------------------------------------------------------------
// Completion
// ---------------------------------------------------------------------------

void ScriptCodeEditor::triggerCompletion() { updateCompletion(true); }

void ScriptCodeEditor::updateCompletion(bool explicitRequest) {
    const QTextCursor cursor = textCursor();
    const QTextBlock block = cursor.block();
    const auto result = core::scripting::complete(toPlainText(), block.blockNumber() + 1,
                                                  cursor.positionInBlock());
    // A space typed into indentation is not a request for statements
    const bool onlyIndentation = block.text().left(cursor.positionInBlock()).trimmed().isEmpty();
    if (result.items.isEmpty() || (!explicitRequest && result.prefix.isEmpty() && onlyIndentation)) {
        m_completer->popup()->hide();
        return;
    }

    m_completionModel->clear();
    for (const auto &item : result.items) {
        auto *row = new QStandardItem(item.label);
        row->setData(item.insertText, Qt::UserRole);
        if (!item.detail.isEmpty())
            row->setToolTip(item.detail);
        row->setEditable(false);
        m_completionModel->appendRow(row);
    }
    m_completionStart = block.position() + result.replaceStart;

    QAbstractItemView *popup = m_completer->popup();
    QRect rect = cursorRect();
    rect.setWidth(popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width() + 16);
    m_completer->complete(rect);
    popup->setCurrentIndex(m_completionModel->index(0, 0));
}

void ScriptCodeEditor::insertCompletion(const QString &insertText) {
    QTextCursor cursor = textCursor();
    cursor.setPosition(m_completionStart, QTextCursor::KeepAnchor);
    cursor.insertText(insertText);

    // Function call: select the first parameter so it can be typed over
    const int open = insertText.indexOf('(');
    if (open >= 0 && insertText.endsWith(')') && insertText.length() > open + 2) {
        int end = insertText.indexOf(',', open);
        if (end < 0)
            end = insertText.length() - 1;
        const int base = m_completionStart;
        cursor.setPosition(base + open + 1);
        cursor.setPosition(base + end, QTextCursor::KeepAnchor);
    }
    setTextCursor(cursor);
}

// ---------------------------------------------------------------------------
// Editing helpers
// ---------------------------------------------------------------------------

void ScriptCodeEditor::keyPressEvent(QKeyEvent *event) {
    QAbstractItemView *popup = m_completer->popup();
    if (popup->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter: case Qt::Key_Return: case Qt::Key_Tab:
        case Qt::Key_Escape: case Qt::Key_Backtab:
            // Handled by the completer's popup
            event->ignore();
            return;
        default:
            break;
        }
    }

    const Qt::KeyboardModifiers mods = event->modifiers();
    if (event->key() == Qt::Key_Space && (mods & Qt::ControlModifier)) {
        triggerCompletion();
        return;
    }
    if (event->key() == Qt::Key_Slash && (mods & Qt::ControlModifier)) {
        toggleComment();
        return;
    }
    if (event->key() == Qt::Key_Tab && !(mods & (Qt::ControlModifier | Qt::AltModifier))) {
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection() && cursor.document()->findBlock(cursor.selectionStart())
                                         != cursor.document()->findBlock(cursor.selectionEnd())) {
            indentSelection(true);
        } else {
            const int col = cursor.positionInBlock();
            cursor.insertText(QString(kIndentWidth - col % kIndentWidth, ' '));
        }
        return;
    }
    if (event->key() == Qt::Key_Backtab) {
        indentSelection(false);
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !(mods & Qt::ShiftModifier)) {
        insertNewlineWithIndent();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);

    const QString typed = event->text();
    if (typed.isEmpty() || mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) && popup->isVisible())
            updateCompletion(false);
        return;
    }
    const QChar ch = typed.back();
    if (ch.isLetter())
        reindentClosingKeyword();
    if (ch.isLetterOrNumber() || ch == '_' || ch == '$' || ch == ' ')
        updateCompletion(false);
    else
        popup->hide();
}

void ScriptCodeEditor::insertNewlineWithIndent() {
    QTextCursor cursor = textCursor();
    const QString line = cursor.block().text();
    QString indent = leadingWhitespace(line);
    if (opensBlock(firstKeyword(line)))
        indent += kIndentUnit;
    cursor.insertText(QStringLiteral("\n") + indent);
    setTextCursor(cursor);
}

void ScriptCodeEditor::reindentClosingKeyword() {
    QTextCursor cursor = textCursor();
    const QTextBlock block = cursor.block();
    const QString line = block.text();
    if (!closesBlock(line.trimmed().toUpper()))
        return;
    // Use the formatter's indentation for this line, so ENDIF lines up with its IF
    const QStringList formatted = core::scripting::formatScript(toPlainText()).split('\n');
    if (block.blockNumber() >= formatted.size())
        return;
    const QString wanted = leadingWhitespace(formatted[block.blockNumber()]);
    const QString current = leadingWhitespace(line);
    if (wanted == current)
        return;
    const int offset = cursor.positionInBlock() - current.length();
    QTextCursor edit(block);
    edit.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, current.length());
    edit.insertText(wanted);
    cursor.setPosition(block.position() + wanted.length() + offset);
    setTextCursor(cursor);
}

void ScriptCodeEditor::indentSelection(bool increase) {
    QTextCursor cursor = textCursor();
    QTextBlock first = document()->findBlock(cursor.selectionStart());
    QTextBlock last = document()->findBlock(cursor.selectionEnd());
    // A selection ending at the start of a line does not include that line
    if (cursor.hasSelection() && last != first && cursor.selectionEnd() == last.position())
        last = last.previous();

    QTextCursor edit(document());
    edit.beginEditBlock();
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        const QString text = b.text();
        QTextCursor lineCursor(b);
        if (increase) {
            if (!text.trimmed().isEmpty())
                lineCursor.insertText(kIndentUnit);
        } else {
            int remove = 0;
            while (remove < kIndentWidth && remove < text.length() && text[remove] == ' ')
                ++remove;
            if (remove == 0 && !text.isEmpty() && text[0] == '\t')
                remove = 1;
            lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, remove);
            lineCursor.removeSelectedText();
        }
        if (b == last)
            break;
    }
    edit.endEditBlock();
}

void ScriptCodeEditor::toggleComment() {
    QTextCursor cursor = textCursor();
    QTextBlock first = document()->findBlock(cursor.selectionStart());
    QTextBlock last = document()->findBlock(cursor.selectionEnd());
    if (cursor.hasSelection() && last != first && cursor.selectionEnd() == last.position())
        last = last.previous();

    // Uncomment only when every non-empty line is already a comment
    bool allCommented = true;
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        const QString t = b.text().trimmed();
        if (!t.isEmpty() && !t.startsWith('#'))
            allCommented = false;
        if (b == last)
            break;
    }

    QTextCursor edit(document());
    edit.beginEditBlock();
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        const QString text = b.text();
        if (!text.trimmed().isEmpty()) {
            const int indent = leadingWhitespace(text).length();
            QTextCursor lineCursor(b);
            lineCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, indent);
            if (allCommented) {
                const int remove = text.mid(indent, 2) == QLatin1String("# ") ? 2 : 1;
                lineCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, remove);
                lineCursor.removeSelectedText();
            } else {
                lineCursor.insertText(QStringLiteral("# "));
            }
        }
        if (b == last)
            break;
    }
    edit.endEditBlock();
}

void ScriptCodeEditor::formatDocument() {
    const QString original = toPlainText();
    const QString formatted = core::scripting::formatScript(original);
    if (formatted == original)
        return;
    const int line = textCursor().blockNumber();
    const int fromEnd = textCursor().block().length() - 1 - textCursor().positionInBlock();

    QTextCursor edit(document());
    edit.beginEditBlock();
    edit.select(QTextCursor::Document);
    edit.insertText(formatted);
    edit.endEditBlock();

    // Keep the cursor on the same line and at the same place in its code
    const QTextBlock block = document()->findBlockByNumber(line);
    if (block.isValid()) {
        QTextCursor cursor(block);
        cursor.setPosition(block.position() + qMax(0, block.length() - 1 - fromEnd));
        setTextCursor(cursor);
    }
}

} // namespace ui::widgets
