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

#include "ui/widgets/QScriptEditorWidget/QScriptEditorWidget.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QTextBlock>
#include <QTextLayout>
#include <QTreeWidget>
#include <QtTest/QtTest>

using namespace ui::widgets;
using core::scripting::language::TokenCategory;

namespace {

// Character format applied by the highlighter at a position of a line
QTextCharFormat formatAt(ScriptCodeEditor &editor, int line, int pos) {
    const QTextBlock block = editor.document()->findBlockByNumber(line);
    for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
        if (pos >= range.start && pos < range.start + range.length)
            return range.format;
    }
    return QTextCharFormat();
}

QColor colorAt(ScriptCodeEditor &editor, int line, int pos) {
    return formatAt(editor, line, pos).foreground().color();
}

QColor categoryColor(ScriptCodeEditor &editor, TokenCategory category) {
    return editor.highlighter()->categoryFormat(category).foreground().color();
}

void setTextAndMoveToEnd(ScriptCodeEditor &editor, const QString &text) {
    editor.setPlainText(text);
    QTextCursor cursor = editor.textCursor();
    cursor.movePosition(QTextCursor::End);
    editor.setTextCursor(cursor);
}

QStringList popupLabels(ScriptCodeEditor &editor) {
    QStringList labels;
    QAbstractItemModel *model = editor.completer()->popup()->model();
    for (int i = 0; i < model->rowCount(); ++i)
        labels << model->index(i, 0).data().toString();
    return labels;
}

} // namespace

class TestScriptEditor : public QObject {
    Q_OBJECT

  private slots:
    void testHighlightsTokensByCategory();
    void testHighlightsCommentsAndMetadata();
    void testInterpolatedVariableInString();
    void testDarkAndLightPalettes();
    void testDiagnosticsUnderlineAndKeepColour();
    void testCompletionPopupAndAccept();
    void testCompletionSelectsFirstParameter();
    void testNoCompletionWhileIndenting();
    void testAutoIndentAfterBlockOpener();
    void testClosingKeywordDedents();
    void testTabAndShiftTab();
    void testToggleComment();
    void testFormatIsUndoable();
    void testProblemsList();
};

void TestScriptEditor::testHighlightsTokensByCategory() {
    ScriptCodeEditor editor;
    editor.setPlainText("PRESS ENTER\nIF $a == 3 THEN\nCALL login($u)\nGOTO done\nPRESS KEY_A");
    QCOMPARE(colorAt(editor, 0, 0), categoryColor(editor, TokenCategory::Command));
    QCOMPARE(colorAt(editor, 0, 6), categoryColor(editor, TokenCategory::AidKey));
    QCOMPARE(colorAt(editor, 1, 0), categoryColor(editor, TokenCategory::Control));
    QCOMPARE(colorAt(editor, 1, 3), categoryColor(editor, TokenCategory::Variable));
    QCOMPARE(colorAt(editor, 1, 6), categoryColor(editor, TokenCategory::Operator));
    QCOMPARE(colorAt(editor, 1, 9), categoryColor(editor, TokenCategory::Number));
    QCOMPARE(colorAt(editor, 2, 5), categoryColor(editor, TokenCategory::Function));
    QCOMPARE(colorAt(editor, 3, 5), categoryColor(editor, TokenCategory::Label));
    QCOMPARE(colorAt(editor, 4, 6), categoryColor(editor, TokenCategory::CharKey));
    // Distinct categories really get distinct colours
    QVERIFY(categoryColor(editor, TokenCategory::Command) != categoryColor(editor, TokenCategory::Control));
}

void TestScriptEditor::testHighlightsCommentsAndMetadata() {
    ScriptCodeEditor editor;
    editor.setPlainText("# @script.name = \"Login\"\nWAIT 10 # pause \"x\"\nTYPE \"a # b\"");
    QCOMPARE(colorAt(editor, 0, 3), editor.highlighter()->metadataFormat().foreground().color());
    QCOMPARE(colorAt(editor, 1, 0), categoryColor(editor, TokenCategory::Command));
    QCOMPARE(colorAt(editor, 1, 10), editor.highlighter()->commentFormat().foreground().color());
    // '#' inside a string is not a comment
    QCOMPARE(colorAt(editor, 2, 8), categoryColor(editor, TokenCategory::String));
}

void TestScriptEditor::testInterpolatedVariableInString() {
    ScriptCodeEditor editor;
    editor.setPlainText("TYPE \"Hi $NAME!\"");
    QCOMPARE(colorAt(editor, 0, 6), categoryColor(editor, TokenCategory::String));
    QCOMPARE(colorAt(editor, 0, 10), categoryColor(editor, TokenCategory::Variable));
    QCOMPARE(colorAt(editor, 0, 15), categoryColor(editor, TokenCategory::String));
}

void TestScriptEditor::testDarkAndLightPalettes() {
    ScriptCodeEditor editor;
    editor.setPlainText("PRESS ENTER");
    editor.highlighter()->setDarkPalette(true);
    QCOMPARE(colorAt(editor, 0, 0), QColor("#4CAF50"));  // VS Code extension colour
    editor.highlighter()->setDarkPalette(false);
    QCOMPARE(colorAt(editor, 0, 0), QColor("#2E7D32"));

    // Follows the widget palette
    QPalette dark = editor.palette();
    dark.setColor(QPalette::Base, QColor("#1e1e1e"));
    editor.setPalette(dark);
    QVERIFY(editor.highlighter()->darkPalette());
    QPalette light = editor.palette();
    light.setColor(QPalette::Base, QColor("#ffffff"));
    editor.setPalette(light);
    QVERIFY(!editor.highlighter()->darkPalette());
}

void TestScriptEditor::testDiagnosticsUnderlineAndKeepColour() {
    ScriptCodeEditor editor;
    QSignalSpy spy(&editor, &ScriptCodeEditor::diagnosticsChanged);
    editor.setPlainText("WAIT 1\nGOTO nowhere");
    // Linting waits for typing to pause
    QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 2000);
    QCOMPARE(editor.diagnostics().size(), 1);
    const QTextCharFormat f = formatAt(editor, 1, 0);
    QCOMPARE(f.underlineStyle(), QTextCharFormat::WaveUnderline);
    QCOMPARE(f.foreground().color(), categoryColor(editor, TokenCategory::Control));
    QVERIFY(formatAt(editor, 0, 0).underlineStyle() != QTextCharFormat::WaveUnderline);

    editor.setPlainText("LABEL nowhere\nGOTO nowhere");
    editor.lintNow();
    QVERIFY(editor.diagnostics().isEmpty());
    QVERIFY(formatAt(editor, 1, 0).underlineStyle() != QTextCharFormat::WaveUnderline);
}

void TestScriptEditor::testCompletionPopupAndAccept() {
    ScriptCodeEditor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    QTest::keyClicks(&editor, "pre");
    QAbstractItemView *popup = editor.completer()->popup();
    QTRY_VERIFY(popup->isVisible());
    QCOMPARE(popupLabels(editor), QStringList{"PRESS"});
    QTest::keyClick(popup, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QString("press"));  // lower-case prefix keeps lower case

    // After the command and a space, keys are offered
    QTest::keyClicks(&editor, " ");
    QTRY_VERIFY(popup->isVisible());
    QVERIFY(popupLabels(editor).contains("ENTER"));
    QTest::keyClicks(&editor, "fieldex");
    QCOMPARE(popupLabels(editor), QStringList{"FIELDEXIT"});
    QTest::keyClick(popup, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), QString("press fieldexit"));

    // Escape closes the list without inserting
    QTest::keyClick(&editor, Qt::Key_Return);
    QTest::keyClicks(&editor, "WA");
    QTRY_VERIFY(popup->isVisible());
    QTest::keyClick(popup, Qt::Key_Escape);
    QVERIFY(!popup->isVisible());
    QCOMPARE(editor.toPlainText(), QString("press fieldexit\nWA"));
}

void TestScriptEditor::testCompletionSelectsFirstParameter() {
    ScriptCodeEditor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    setTextAndMoveToEnd(editor, "DEF login($user, $pass)\nENDDEF\nCALL ");
    editor.triggerCompletion();
    QAbstractItemView *popup = editor.completer()->popup();
    QTRY_VERIFY(popup->isVisible());
    QCOMPARE(popupLabels(editor), QStringList{"login"});
    QTest::keyClick(popup, Qt::Key_Return);
    QCOMPARE(editor.document()->lastBlock().text(), QString("CALL login($user, $pass)"));
    QCOMPARE(editor.textCursor().selectedText(), QString("$user"));
}

void TestScriptEditor::testNoCompletionWhileIndenting() {
    ScriptCodeEditor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    QTest::keyClicks(&editor, "  ");
    QVERIFY(!editor.completer()->popup()->isVisible());
    // Ctrl+Space still opens the statement list
    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    QTRY_VERIFY(editor.completer()->popup()->isVisible());
    QVERIFY(popupLabels(editor).contains("TYPE"));
}

void TestScriptEditor::testAutoIndentAfterBlockOpener() {
    ScriptCodeEditor editor;
    setTextAndMoveToEnd(editor, "  IF $a == 1");
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QString("  IF $a == 1\n      "));
    setTextAndMoveToEnd(editor, "    WAIT 1");
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QString("    WAIT 1\n    "));
}

void TestScriptEditor::testClosingKeywordDedents() {
    ScriptCodeEditor editor;
    setTextAndMoveToEnd(editor, "WHILE $i < 3\n    INC $i\n    ENDWHIL");
    QTest::keyClicks(&editor, "E");
    QCOMPARE(editor.toPlainText(), QString("WHILE $i < 3\n    INC $i\nENDWHILE"));
    QCOMPARE(editor.textCursor().positionInBlock(), 8);  // cursor stays after the word
}

void TestScriptEditor::testTabAndShiftTab() {
    ScriptCodeEditor editor;
    setTextAndMoveToEnd(editor, "WA");
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), QString("WA  "));  // to the next 4-column stop

    editor.setPlainText("WAIT 1\nWAIT 2\nWAIT 3");
    editor.selectAll();
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), QString("    WAIT 1\n    WAIT 2\n    WAIT 3"));
    QTest::keyClick(&editor, Qt::Key_Backtab);
    QCOMPARE(editor.toPlainText(), QString("WAIT 1\nWAIT 2\nWAIT 3"));
}

void TestScriptEditor::testToggleComment() {
    ScriptCodeEditor editor;
    editor.setPlainText("IF $a == 1\n    WAIT 1\n\nENDIF");
    editor.selectAll();
    editor.toggleComment();
    QCOMPARE(editor.toPlainText(), QString("# IF $a == 1\n    # WAIT 1\n\n# ENDIF"));
    editor.selectAll();
    QTest::keyClick(&editor, Qt::Key_Slash, Qt::ControlModifier);
    QCOMPARE(editor.toPlainText(), QString("IF $a == 1\n    WAIT 1\n\nENDIF"));
}

void TestScriptEditor::testFormatIsUndoable() {
    ScriptCodeEditor editor;
    const QString original = "IF $a == 1\nWAIT 1   \nENDIF";
    editor.setPlainText(original);
    editor.formatDocument();
    QCOMPARE(editor.toPlainText(), QString("IF $a == 1\n    WAIT 1\nENDIF"));
    editor.undo();
    QCOMPARE(editor.toPlainText(), original);
}

void TestScriptEditor::testProblemsList() {
    QScriptEditorWidget widget;
    QSignalSpy spy(&widget, &QScriptEditorWidget::problemsChanged);
    widget.setText("SET $unused 1\nWAIT 1\nGOTO nowhere\n");
    QCOMPARE(widget.errorCount(), 1);
    QCOMPARE(widget.warningCount(), 1);
    QVERIFY(!spy.isEmpty());
    QTreeWidget *list = widget.problemsList();
    QCOMPARE(list->topLevelItemCount(), 2);
    QCOMPARE(list->topLevelItem(1)->text(0), QString("3"));

    emit list->itemActivated(list->topLevelItem(1), 0);
    QCOMPARE(widget.editor()->textCursor().blockNumber(), 2);
}

QTEST_MAIN(TestScriptEditor)
#include "test_script_editor.moc"
