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

#include <QtUiStyle/BaseFramelessDialog.h>

namespace ui::widgets { class QScriptEditorWidget; }

// Modal dialog with the built-in script editor. Used where a separate
// window would be blocked by a modal parent (the Connect dialog).
class ScriptEditorDialog : public qt_ui_style::BaseFramelessDialog {
    Q_OBJECT

  public:
    explicit ScriptEditorDialog(const QString &title, const QString &text, QWidget *parent = nullptr);

    QString text() const;
    ui::widgets::QScriptEditorWidget *editorWidget() const { return m_editor; }

    // Show the dialog; on OK, replace `text` and return true
    static bool edit(QWidget *parent, const QString &title, QString &text);

  private:
    ui::widgets::QScriptEditorWidget *m_editor = nullptr;
};
