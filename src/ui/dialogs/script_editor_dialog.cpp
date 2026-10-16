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

#include "script_editor_dialog.h"
#include "ui/widgets/QScriptEditorWidget/QScriptEditorWidget.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

using ui::widgets::QScriptEditorWidget;

ScriptEditorDialog::ScriptEditorDialog(const QString &title, const QString &text, QWidget *parent)
    : qt_ui_style::BaseFramelessDialog(parent) {
    setWindowTitle(title);
    setModal(true);
    resize(820, 600);

    QVBoxLayout *layout = contentLayout();
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_editor = new QScriptEditorWidget(this);
    m_editor->setText(text);
    layout->addWidget(m_editor, 1);

    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *format = new QPushButton(tr("Format"), this);
    format->setToolTip(tr("Re-indent the script"));
    QPushButton *ok = new QPushButton(tr("Save"), this);
    ok->setDefault(true);
    QPushButton *cancel = new QPushButton(tr("Cancel"), this);
    buttons->addWidget(format);
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);

    connect(format, &QPushButton::clicked, m_editor->editor(), &ui::widgets::ScriptCodeEditor::formatDocument);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    m_editor->editor()->setFocus();
}

QString ScriptEditorDialog::text() const { return m_editor->text(); }

bool ScriptEditorDialog::edit(QWidget *parent, const QString &title, QString &text) {
    ScriptEditorDialog dialog(title, text, parent);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    text = dialog.text();
    return true;
}
