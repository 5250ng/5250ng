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

#include <QPointer>
#include <QtUiStyle/BaseFramelessWindow.h>
#include <functional>

class QFileSystemWatcher;
class QLabel;
class QPushButton;
class QTabWidget;

namespace ui::widgets { class QScriptEditorWidget; }

// Tabbed window for editing .5250script files with the built-in editor.
// One shared instance is used by the application (see instance()).
class ScriptEditorWindow : public qt_ui_style::BaseFramelessWindow {
    Q_OBJECT

  public:
    enum class EditorPreference { BuiltIn, External };
    enum class SaveChoice { Save, Discard, Cancel };

    explicit ScriptEditorWindow(QWidget *parent = nullptr);

    // Shared window, created on first use as a child window of the
    // default parent (the main window) so it closes with the application
    static ScriptEditorWindow *instance();
    static ScriptEditorWindow *existingInstance();
    static void setDefaultParent(QWidget *parent);

    // "Edit scripts with" setting (Settings > Scripts), stored in QSettings
    static EditorPreference editorPreference();
    static void setEditorPreference(EditorPreference preference);

    // Open a script for editing in the built-in window or the system's
    // default application, following the setting
    static void editScript(const QString &path);

    // Called by Run/Stop; registered by the main window
    static void setRunHandler(std::function<void(const QString &path)> handler);
    static void setStopHandler(std::function<void()> handler);

    // Open a file in a tab (or switch to it if already open). Returns false
    // and shows a warning if the file can't be read.
    bool openFile(const QString &path);
    // New untitled script from the starter template
    void newScript();

    int tabCount() const;
    ui::widgets::QScriptEditorWidget *currentEditor() const;
    ui::widgets::QScriptEditorWidget *editorAt(int index) const;
    QString pathAt(int index) const;  // empty for an untitled tab

    bool saveTab(int index);
    bool saveTabAs(int index);
    // Ask about unsaved changes, then close. Returns false if cancelled.
    bool closeTab(int index);
    // Ask about every tab with unsaved changes. Returns false if cancelled.
    bool confirmCloseAll();

    // Overridable for tests: asks whether to save changes to `name`
    std::function<SaveChoice(const QString &name)> askSaveChanges;

  protected:
    void closeEvent(QCloseEvent *event) override;

  private:
    int addTab(const QString &path, const QString &text);
    int indexOfPath(const QString &path) const;
    void updateTabTitle(int index);
    void updateStatus();
    void onFileChanged(const QString &path);
    void runCurrent();
    void openInExternalEditor();
    bool writeFile(int index, const QString &path);
    SaveChoice defaultAskSaveChanges(const QString &name);

    QTabWidget *m_tabs = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_runButton = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    int m_untitledCounter = 0;
};
