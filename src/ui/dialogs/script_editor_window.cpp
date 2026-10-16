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

#include "script_editor_window.h"
#include "core/script_library.h"
#include "ui/widgets/QScriptEditorWidget/QScriptEditorWidget.h"
#include <5250script/script_compiler.h>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QTabWidget>
#include <QTextBlock>
#include <QUrl>
#include <QVBoxLayout>
#include <QtUiStyle/BaseFramelessDialog.h>
#include <QtUiStyle/StyledMessageBox.h>

using ui::widgets::QScriptEditorWidget;

namespace {

constexpr const char *kPathProperty = "scriptPath";
constexpr const char *kUntitledProperty = "untitledName";
const QString kPreferenceKey = QStringLiteral("Scripts/editor");

QPointer<ScriptEditorWindow> s_instance;
QPointer<QWidget> s_defaultParent;
std::function<void(const QString &)> s_runHandler;
std::function<void()> s_stopHandler;

QString starterTemplate(const QString &name) {
    return QString(
        "# @script.name = \"%1\"\n"
        "# @script.description = \"\"\n"
        "# @script.version = \"1.0\"\n"
        "# @menu.path = \"%1\"\n"
        "\n"
        "EXPECT KEYBOARD UNLOCKED\n").arg(name);
}

} // namespace

ScriptEditorWindow::ScriptEditorWindow(QWidget *parent)
    : qt_ui_style::BaseFramelessWindow(parent) {
    setWindowTitle(tr("Script Editor"));
    resize(900, 650);
    askSaveChanges = [this](const QString &name) { return defaultAskSaveChanges(name); };

    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(8, 8, 8, 8);
    content->setSpacing(6);

    // Toolbar row. Each button has a window-wide shortcut so they don't
    // reach (or steal keys from) the terminal in the main window.
    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);
    auto addButton = [this, toolbar](const QString &text, const QKeySequence &shortcut,
                                     const QString &tip, auto slot) {
        QPushButton *button = new QPushButton(text, this);
        button->setToolTip(shortcut.isEmpty() ? tip
                                              : QString("%1 (%2)").arg(tip, shortcut.toString(QKeySequence::NativeText)));
        connect(button, &QPushButton::clicked, this, slot);
        if (!shortcut.isEmpty()) {
            QAction *action = new QAction(this);
            action->setShortcut(shortcut);
            action->setShortcutContext(Qt::WindowShortcut);
            connect(action, &QAction::triggered, this, slot);
            addAction(action);
        }
        toolbar->addWidget(button);
        return button;
    };
    addButton(tr("New"), QKeySequence::New, tr("New script"), [this]() { newScript(); });
    addButton(tr("Open..."), QKeySequence::Open, tr("Open a script file"), [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Open Script"), core::scriptsDirectory(),
            tr("5250 Scripts (*.5250script);;All Files (*)"));
        if (!path.isEmpty())
            openFile(path);
    });
    addButton(tr("Save"), QKeySequence::Save, tr("Save"), [this]() { saveTab(m_tabs->currentIndex()); });
    addButton(tr("Save As..."), QKeySequence::SaveAs, tr("Save under a new name"),
              [this]() { saveTabAs(m_tabs->currentIndex()); });
    toolbar->addSpacing(12);
    addButton(tr("Format"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), tr("Re-indent the script"),
              [this]() {
                  if (auto *editor = currentEditor())
                      editor->editor()->formatDocument();
              });
    toolbar->addSpacing(12);
    m_runButton = addButton(tr("Run"), QKeySequence(Qt::Key_F5),
                            tr("Save, then run on the active session"), [this]() { runCurrent(); });
    addButton(tr("Stop"), QKeySequence(Qt::SHIFT | Qt::Key_F5), tr("Stop the running script"), []() {
        if (s_stopHandler)
            s_stopHandler();
    });
    toolbar->addStretch();
    addButton(tr("Open Externally"), QKeySequence(), tr("Open the file in the system's default application"),
              [this]() { openInExternalEditor(); });
    content->addLayout(toolbar);

    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    content->addWidget(m_tabs, 1);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) { closeTab(index); });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this]() { updateStatus(); });

    QAction *closeAction = new QAction(this);
    closeAction->setShortcut(QKeySequence::Close);
    closeAction->setShortcutContext(Qt::WindowShortcut);
    connect(closeAction, &QAction::triggered, this, [this]() {
        if (m_tabs->count() > 0)
            closeTab(m_tabs->currentIndex());
    });
    addAction(closeAction);

    m_status = new QLabel(this);
    content->addWidget(m_status);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &ScriptEditorWindow::onFileChanged);
    updateStatus();
}

// ---------------------------------------------------------------------------
// Shared instance and preferences
// ---------------------------------------------------------------------------

ScriptEditorWindow *ScriptEditorWindow::instance() {
    if (!s_instance)
        s_instance = new ScriptEditorWindow(s_defaultParent);
    return s_instance;
}

ScriptEditorWindow *ScriptEditorWindow::existingInstance() { return s_instance; }

void ScriptEditorWindow::setDefaultParent(QWidget *parent) { s_defaultParent = parent; }

ScriptEditorWindow::EditorPreference ScriptEditorWindow::editorPreference() {
    return QSettings().value(kPreferenceKey).toString() == QLatin1String("external")
               ? EditorPreference::External
               : EditorPreference::BuiltIn;
}

void ScriptEditorWindow::setEditorPreference(EditorPreference preference) {
    QSettings().setValue(kPreferenceKey, preference == EditorPreference::External
                                             ? QStringLiteral("external")
                                             : QStringLiteral("builtin"));
}

void ScriptEditorWindow::editScript(const QString &path) {
    if (editorPreference() == EditorPreference::External) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return;
    }
    ScriptEditorWindow *window = instance();
    window->openFile(path);
    window->show();
    window->raise();
    window->activateWindow();
}

void ScriptEditorWindow::setRunHandler(std::function<void(const QString &)> handler) {
    s_runHandler = std::move(handler);
}

void ScriptEditorWindow::setStopHandler(std::function<void()> handler) {
    s_stopHandler = std::move(handler);
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

int ScriptEditorWindow::tabCount() const { return m_tabs->count(); }

QScriptEditorWidget *ScriptEditorWindow::editorAt(int index) const {
    return qobject_cast<QScriptEditorWidget *>(m_tabs->widget(index));
}

QScriptEditorWidget *ScriptEditorWindow::currentEditor() const { return editorAt(m_tabs->currentIndex()); }

QString ScriptEditorWindow::pathAt(int index) const {
    QWidget *w = m_tabs->widget(index);
    return w ? w->property(kPathProperty).toString() : QString();
}

int ScriptEditorWindow::indexOfPath(const QString &path) const {
    const QString wanted = QFileInfo(path).absoluteFilePath();
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (!pathAt(i).isEmpty() && pathAt(i) == wanted)
            return i;
    }
    return -1;
}

bool ScriptEditorWindow::openFile(const QString &path) {
    const int existing = indexOfPath(path);
    if (existing >= 0) {
        m_tabs->setCurrentIndex(existing);
        return true;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qt_ui_style::StyledMessageBox::warning(this, tr("Open Script"),
            tr("Cannot read %1:\n%2").arg(path, file.errorString()));
        return false;
    }
    const QString text = QString::fromUtf8(file.readAll());
    const int index = addTab(QFileInfo(path).absoluteFilePath(), text);
    m_tabs->setCurrentIndex(index);
    return true;
}

void ScriptEditorWindow::newScript() {
    const QString name = tr("untitled-%1").arg(++m_untitledCounter);
    const int index = addTab(QString(), starterTemplate(name));
    m_tabs->widget(index)->setProperty(kUntitledProperty, name);
    updateTabTitle(index);
    m_tabs->setCurrentIndex(index);
    QScriptEditorWidget *editor = editorAt(index);
    editor->editor()->moveCursor(QTextCursor::End);
    editor->editor()->setFocus();
}

int ScriptEditorWindow::addTab(const QString &path, const QString &text) {
    QScriptEditorWidget *editor = new QScriptEditorWidget(m_tabs);
    editor->setText(text);
    editor->editor()->document()->setModified(false);
    editor->setProperty(kPathProperty, path);
    const int index = m_tabs->addTab(editor, QString());
    if (!path.isEmpty())
        m_watcher->addPath(path);

    connect(editor->editor()->document(), &QTextDocument::modificationChanged, this, [this, editor]() {
        updateTabTitle(m_tabs->indexOf(editor));
    });
    connect(editor->editor(), &QPlainTextEdit::cursorPositionChanged, this, [this, editor]() {
        if (editor == currentEditor())
            updateStatus();
    });
    connect(editor, &QScriptEditorWidget::problemsChanged, this, [this, editor]() {
        if (editor == currentEditor())
            updateStatus();
    });
    updateTabTitle(index);
    return index;
}

void ScriptEditorWindow::updateTabTitle(int index) {
    QWidget *w = m_tabs->widget(index);
    if (!w)
        return;
    const QString path = pathAt(index);
    QString title = path.isEmpty() ? w->property(kUntitledProperty).toString() : QFileInfo(path).fileName();
    if (editorAt(index)->editor()->document()->isModified())
        title += QStringLiteral(" ●");
    m_tabs->setTabText(index, title);
    m_tabs->setTabToolTip(index, path.isEmpty() ? tr("Not saved yet") : path);
}

void ScriptEditorWindow::updateStatus() {
    QScriptEditorWidget *editor = currentEditor();
    m_runButton->setEnabled(editor != nullptr);
    if (!editor) {
        m_status->setText(tr("No script open"));
        return;
    }
    const QTextCursor cursor = editor->editor()->textCursor();
    QStringList parts;
    parts << tr("Ln %1, Col %2").arg(cursor.blockNumber() + 1).arg(cursor.positionInBlock() + 1);
    const QString name = core::scripting::ScriptCompiler::extractMetadata(editor->text()).name();
    if (!name.isEmpty())
        parts << name;
    if (editor->errorCount() || editor->warningCount())
        parts << tr("%1 error(s), %2 warning(s)").arg(editor->errorCount()).arg(editor->warningCount());
    else
        parts << tr("No problems");
    m_status->setText(parts.join(QStringLiteral("  ·  ")));
}

// ---------------------------------------------------------------------------
// Saving and closing
// ---------------------------------------------------------------------------

bool ScriptEditorWindow::writeFile(int index, const QString &path) {
    QScriptEditorWidget *editor = editorAt(index);
    // Our own write must not look like an external change
    m_watcher->removePath(path);
    QSaveFile file(path);
    bool ok = file.open(QIODevice::WriteOnly | QIODevice::Text);
    if (ok) {
        file.write(editor->text().toUtf8());
        ok = file.commit();
    }
    m_watcher->addPath(path);
    if (!ok) {
        qt_ui_style::StyledMessageBox::warning(this, tr("Save Script"),
            tr("Cannot write %1:\n%2").arg(path, file.errorString()));
        return false;
    }
    editor->editor()->document()->setModified(false);
    return true;
}

bool ScriptEditorWindow::saveTab(int index) {
    if (!editorAt(index))
        return false;
    const QString path = pathAt(index);
    if (path.isEmpty())
        return saveTabAs(index);
    return writeFile(index, path);
}

bool ScriptEditorWindow::saveTabAs(int index) {
    QWidget *w = m_tabs->widget(index);
    if (!w)
        return false;
    QString suggested = pathAt(index);
    if (suggested.isEmpty())
        suggested = core::scriptsDirectory() + "/" + w->property(kUntitledProperty).toString() + ".5250script";
    QString path = QFileDialog::getSaveFileName(this, tr("Save Script As"), suggested,
                                                tr("5250 Scripts (*.5250script);;All Files (*)"));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".5250script");
    path = QFileInfo(path).absoluteFilePath();
    const QString old = pathAt(index);
    if (!writeFile(index, path))
        return false;
    if (!old.isEmpty() && old != path)
        m_watcher->removePath(old);
    w->setProperty(kPathProperty, path);
    updateTabTitle(index);
    return true;
}

ScriptEditorWindow::SaveChoice ScriptEditorWindow::defaultAskSaveChanges(const QString &name) {
    auto *dialog = new qt_ui_style::BaseFramelessDialog(this);
    dialog->setWindowTitle(tr("Unsaved Changes"));
    dialog->contentLayout()->setContentsMargins(12, 8, 12, 12);
    dialog->contentLayout()->setSpacing(10);
    QLabel *label = new QLabel(tr("Save changes to %1 before closing?").arg(name), dialog);
    dialog->contentLayout()->addWidget(label);
    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *save = new QPushButton(tr("Save"), dialog);
    QPushButton *discard = new QPushButton(tr("Don't Save"), dialog);
    QPushButton *cancel = new QPushButton(tr("Cancel"), dialog);
    save->setDefault(true);
    buttons->addStretch();
    buttons->addWidget(save);
    buttons->addWidget(discard);
    buttons->addWidget(cancel);
    dialog->contentLayout()->addLayout(buttons);
    SaveChoice choice = SaveChoice::Cancel;
    connect(save, &QPushButton::clicked, dialog, [&]() { choice = SaveChoice::Save; dialog->accept(); });
    connect(discard, &QPushButton::clicked, dialog, [&]() { choice = SaveChoice::Discard; dialog->accept(); });
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::reject);
    dialog->exec();
    delete dialog;
    return choice;
}

bool ScriptEditorWindow::closeTab(int index) {
    QScriptEditorWidget *editor = editorAt(index);
    if (!editor)
        return true;
    if (editor->editor()->document()->isModified()) {
        m_tabs->setCurrentIndex(index);
        QString name = m_tabs->tabText(index);
        name.remove(QStringLiteral(" ●"));
        switch (askSaveChanges(name)) {
        case SaveChoice::Save:
            if (!saveTab(index))
                return false;
            break;
        case SaveChoice::Discard:
            break;
        case SaveChoice::Cancel:
            return false;
        }
    }
    const QString path = pathAt(index);
    if (!path.isEmpty())
        m_watcher->removePath(path);
    m_tabs->removeTab(index);
    editor->deleteLater();
    updateStatus();
    return true;
}

bool ScriptEditorWindow::confirmCloseAll() {
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        if (!closeTab(i))
            return false;
    }
    return true;
}

void ScriptEditorWindow::closeEvent(QCloseEvent *event) {
    if (confirmCloseAll())
        event->accept();
    else
        event->ignore();
}

// ---------------------------------------------------------------------------
// External changes, running and external editors
// ---------------------------------------------------------------------------

void ScriptEditorWindow::onFileChanged(const QString &path) {
    const int index = indexOfPath(path);
    if (index < 0)
        return;
    // Editors that save by replacing the file drop the watch; re-add it
    if (QFileInfo::exists(path) && !m_watcher->files().contains(path))
        m_watcher->addPath(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;  // deleted or unreadable: keep the open text
    const QString text = QString::fromUtf8(file.readAll());
    QScriptEditorWidget *editor = editorAt(index);
    if (text == editor->text())
        return;
    if (editor->editor()->document()->isModified()) {
        const auto answer = qt_ui_style::StyledMessageBox::question(
            this, tr("File Changed"),
            tr("%1 was changed outside the editor.\nReload it and lose your changes?")
                .arg(QFileInfo(path).fileName()));
        if (answer != qt_ui_style::StyledMessageBox::Yes)
            return;
    }
    const int line = editor->editor()->textCursor().blockNumber();
    editor->setText(text);
    editor->editor()->document()->setModified(false);
    editor->editor()->goToLine(line + 1);
}

void ScriptEditorWindow::runCurrent() {
    const int index = m_tabs->currentIndex();
    QScriptEditorWidget *editor = editorAt(index);
    if (!editor)
        return;
    editor->editor()->lintNow();
    if (editor->errorCount() > 0) {
        qt_ui_style::StyledMessageBox::warning(this, tr("Run Script"),
            tr("Fix the %1 error(s) listed under Problems before running the script.")
                .arg(editor->errorCount()));
        return;
    }
    if ((editor->editor()->document()->isModified() || pathAt(index).isEmpty()) && !saveTab(index))
        return;
    if (s_runHandler)
        s_runHandler(pathAt(index));
}

void ScriptEditorWindow::openInExternalEditor() {
    const int index = m_tabs->currentIndex();
    if (!editorAt(index))
        return;
    if ((editorAt(index)->editor()->document()->isModified() || pathAt(index).isEmpty()) && !saveTab(index))
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(pathAt(index)));
}
