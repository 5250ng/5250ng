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

#include "ui/dialogs/script_editor_dialog.h"
#include "ui/dialogs/script_editor_window.h"
#include "ui/widgets/QScriptEditorWidget/QScriptEditorWidget.h"
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using ui::widgets::QScriptEditorWidget;

namespace {

QString writeScript(const QTemporaryDir &dir, const QString &name, const QString &text) {
    const QString path = dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(text.toUtf8());
    return path;
}

QString readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()) : QString();
}

QPushButton *button(ScriptEditorWindow &window, const QString &text) {
    for (QPushButton *b : window.findChildren<QPushButton *>()) {
        if (b->text() == text)
            return b;
    }
    return nullptr;
}

QString tabTitle(ScriptEditorWindow &window, int index) {
    return window.findChild<QTabWidget *>()->tabText(index);
}

} // namespace

class TestScriptEditorWindow : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void testOpenFileOnce();
    void testModifiedMarkerAndSave();
    void testCloseTabChoices();
    void testReloadsExternalChanges();
    void testRunSavesFirstAndIsBlockedByErrors();
    void testNewScriptUsesTemplate();
    void testEditorPreference();
    void testModalEditDialog();

  private:
    QTemporaryDir m_settingsDir;
};

void TestScriptEditorWindow::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
}

void TestScriptEditorWindow::testOpenFileOnce() {
    QTemporaryDir dir;
    const QString path = writeScript(dir, "a.5250script", "WAIT 1\n");
    ScriptEditorWindow window;
    QVERIFY(window.openFile(path));
    QVERIFY(window.openFile(path));  // switches to the existing tab
    QCOMPARE(window.tabCount(), 1);
    QCOMPARE(window.currentEditor()->text(), QString("WAIT 1\n"));
    QCOMPARE(window.pathAt(0), QFileInfo(path).absoluteFilePath());
    QCOMPARE(tabTitle(window, 0), QString("a.5250script"));
}

void TestScriptEditorWindow::testModifiedMarkerAndSave() {
    QTemporaryDir dir;
    const QString path = writeScript(dir, "b.5250script", "WAIT 1\n");
    ScriptEditorWindow window;
    QVERIFY(window.openFile(path));
    QScriptEditorWidget *editor = window.currentEditor();
    editor->editor()->moveCursor(QTextCursor::End);
    editor->editor()->insertPlainText("PRESS ENTER\n");
    QVERIFY(tabTitle(window, 0).endsWith(QStringLiteral("●")));
    QVERIFY(window.saveTab(0));
    QCOMPARE(readFile(path), QString("WAIT 1\nPRESS ENTER\n"));
    QCOMPARE(tabTitle(window, 0), QString("b.5250script"));
}

void TestScriptEditorWindow::testCloseTabChoices() {
    QTemporaryDir dir;
    const QString path = writeScript(dir, "c.5250script", "WAIT 1\n");
    ScriptEditorWindow window;
    QStringList asked;
    ScriptEditorWindow::SaveChoice answer = ScriptEditorWindow::SaveChoice::Cancel;
    window.askSaveChanges = [&](const QString &name) {
        asked << name;
        return answer;
    };
    QVERIFY(window.openFile(path));
    window.currentEditor()->editor()->insertPlainText("# edited\n");

    QVERIFY(!window.closeTab(0));  // Cancel keeps the tab
    QCOMPARE(window.tabCount(), 1);
    QCOMPARE(asked, QStringList{"c.5250script"});

    answer = ScriptEditorWindow::SaveChoice::Save;
    QVERIFY(window.closeTab(0));
    QCOMPARE(window.tabCount(), 0);
    QVERIFY(readFile(path).startsWith("# edited\n"));

    QVERIFY(window.openFile(path));
    window.currentEditor()->editor()->insertPlainText("# dropped\n");
    answer = ScriptEditorWindow::SaveChoice::Discard;
    QVERIFY(window.confirmCloseAll());
    QCOMPARE(window.tabCount(), 0);
    QVERIFY(!readFile(path).contains("dropped"));

    // Unmodified tabs close without asking
    asked.clear();
    QVERIFY(window.openFile(path));
    QVERIFY(window.closeTab(0));
    QVERIFY(asked.isEmpty());
}

void TestScriptEditorWindow::testReloadsExternalChanges() {
    QTemporaryDir dir;
    const QString path = writeScript(dir, "d.5250script", "WAIT 1\n");
    ScriptEditorWindow window;
    QVERIFY(window.openFile(path));
    QTest::qWait(100);
    writeScript(dir, "d.5250script", "WAIT 2\n");
    QTRY_COMPARE_WITH_TIMEOUT(window.currentEditor()->text(), QString("WAIT 2\n"), 5000);
    QVERIFY(!window.currentEditor()->editor()->document()->isModified());

    // Saving from the editor is not mistaken for an external change
    window.currentEditor()->editor()->insertPlainText("# mine\n");
    QVERIFY(window.saveTab(0));
    QTest::qWait(300);
    QCOMPARE(window.currentEditor()->text(), QString("# mine\nWAIT 2\n"));
}

void TestScriptEditorWindow::testRunSavesFirstAndIsBlockedByErrors() {
    QTemporaryDir dir;
    const QString path = writeScript(dir, "e.5250script", "WAIT 1\n");
    QStringList ran;
    ScriptEditorWindow::setRunHandler([&](const QString &p) { ran << p; });
    ScriptEditorWindow window;
    window.show();
    QVERIFY(window.openFile(path));
    window.currentEditor()->editor()->insertPlainText("PRESS ENTER\n");
    button(window, "Run")->click();
    QCOMPARE(ran, QStringList{QFileInfo(path).absoluteFilePath()});
    QCOMPARE(readFile(path), QString("PRESS ENTER\nWAIT 1\n"));  // saved before running

    // A script with errors is not run (the warning box is closed by the timer)
    window.currentEditor()->editor()->insertPlainText("GOTO nowhere\n");
    QTimer::singleShot(200, []() {
        if (QWidget *modal = QApplication::activeModalWidget())
            modal->close();
    });
    button(window, "Run")->click();
    QCOMPARE(ran.size(), 1);
    ScriptEditorWindow::setRunHandler(nullptr);
}

void TestScriptEditorWindow::testNewScriptUsesTemplate() {
    ScriptEditorWindow window;
    window.newScript();
    window.newScript();
    QCOMPARE(window.tabCount(), 2);
    QVERIFY(window.pathAt(0).isEmpty());
    QCOMPARE(tabTitle(window, 0), QString("untitled-1"));
    QCOMPARE(tabTitle(window, 1), QString("untitled-2"));
    QVERIFY(window.currentEditor()->text().contains("@script.name = \"untitled-2\""));
    QCOMPARE(window.currentEditor()->errorCount(), 0);
}

void TestScriptEditorWindow::testEditorPreference() {
    QCOMPARE(ScriptEditorWindow::editorPreference(), ScriptEditorWindow::EditorPreference::BuiltIn);
    ScriptEditorWindow::setEditorPreference(ScriptEditorWindow::EditorPreference::External);
    QCOMPARE(ScriptEditorWindow::editorPreference(), ScriptEditorWindow::EditorPreference::External);
    ScriptEditorWindow::setEditorPreference(ScriptEditorWindow::EditorPreference::BuiltIn);
    QCOMPARE(ScriptEditorWindow::editorPreference(), ScriptEditorWindow::EditorPreference::BuiltIn);

    // The built-in path opens the shared window
    QTemporaryDir dir;
    const QString path = writeScript(dir, "f.5250script", "WAIT 1\n");
    ScriptEditorWindow::editScript(path);
    ScriptEditorWindow *shared = ScriptEditorWindow::existingInstance();
    QVERIFY(shared);
    QCOMPARE(shared->pathAt(0), QFileInfo(path).absoluteFilePath());
    QVERIFY(shared->isVisible());
    delete shared;
}

void TestScriptEditorWindow::testModalEditDialog() {
    // Accept: the edited text is returned
    QString text = "WAIT 1\n";
    QTimer::singleShot(0, []() {
        auto *dialog = qobject_cast<ScriptEditorDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        QCOMPARE(dialog->editorWidget()->text(), QString("WAIT 1\n"));
        dialog->editorWidget()->editor()->moveCursor(QTextCursor::End);
        dialog->editorWidget()->editor()->insertPlainText("PRESS ENTER\n");
        dialog->accept();
    });
    QVERIFY(ScriptEditorDialog::edit(nullptr, "Edit", text));
    QCOMPARE(text, QString("WAIT 1\nPRESS ENTER\n"));

    // Cancel: the text is left alone
    QTimer::singleShot(0, []() {
        auto *dialog = qobject_cast<ScriptEditorDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->editorWidget()->editor()->insertPlainText("# dropped\n");
        dialog->reject();
    });
    QVERIFY(!ScriptEditorDialog::edit(nullptr, "Edit", text));
    QCOMPARE(text, QString("WAIT 1\nPRESS ENTER\n"));
}

QTEST_MAIN(TestScriptEditorWindow)
#include "test_script_editor_window.moc"
