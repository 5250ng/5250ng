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

#include "../main_window.h"
#include "ui/dialogs/script_editor_window.h"
#include <QCloseEvent>
#include <QtUiStyle/StyledMessageBox.h>

// Connect the shared Script Editor window to this window: it closes with
// the application, and its Run/Stop buttons drive the active session.
void MainWindow::setupScriptEditor() {
    ScriptEditorWindow::setDefaultParent(this);
    ScriptEditorWindow::setRunHandler([this](const QString &path) {
        if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) {
            QWidget *editor = ScriptEditorWindow::existingInstance();
            qt_ui_style::StyledMessageBox::information(
                editor ? editor : static_cast<QWidget *>(this), "Run Script",
                "Connect a session first: scripts run on the active session tab.");
            return;
        }
        onRunScript(path);
    });
    ScriptEditorWindow::setStopHandler([this]() { onStopExecution(); });
}

void MainWindow::onOpenScriptEditor() {
    ScriptEditorWindow *editor = ScriptEditorWindow::instance();
    if (editor->tabCount() == 0)
        editor->newScript();
    editor->show();
    editor->raise();
    editor->activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Give unsaved scripts a chance to be saved before the editor closes
    // along with this window
    if (ScriptEditorWindow *editor = ScriptEditorWindow::existingInstance()) {
        if (!editor->confirmCloseAll()) {
            event->ignore();
            return;
        }
    }
    MainWindowBase::closeEvent(event);
}
