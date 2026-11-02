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

#include "script_input_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui::dialogs {

ScriptInputDialog::ScriptInputDialog(const QStringList &labels, const QList<bool> &masked,
                                     QWidget *parent)
    : qt_ui_style::BaseFramelessDialog(parent) {
    setWindowTitle("Script Input");
    setFixedWidth(400);
    contentLayout()->setContentsMargins(12, 8, 12, 12);
    contentLayout()->setSpacing(8);

    for (int i = 0; i < labels.size(); ++i) {
        auto *row = new QHBoxLayout();
        auto *lbl = new QLabel(labels[i], this);
        lbl->setMinimumWidth(120);
        auto *edit = new QLineEdit(this);
        if (masked.value(i, false))
            edit->setEchoMode(QLineEdit::Password);
        row->addWidget(lbl);
        row->addWidget(edit);
        contentLayout()->addLayout(row);
        m_fields.append(edit);
    }

    auto *btnLayout = new QHBoxLayout();
    auto *okBtn = new QPushButton("OK", this);
    auto *cancelBtn = new QPushButton("Cancel", this);
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    contentLayout()->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    if (!m_fields.isEmpty()) {
        connect(m_fields.last(), &QLineEdit::returnPressed, this, &QDialog::accept);
        m_fields.first()->setFocus();
    }
}

QStringList ScriptInputDialog::values() const {
    QStringList result;
    for (auto *edit : m_fields)
        result.append(edit->text());
    return result;
}

bool ScriptInputDialog::ask(const QStringList &labels, const QList<bool> &masked,
                            QStringList *values, QWidget *parent) {
    ScriptInputDialog dlg(labels, masked, parent);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    if (values) *values = dlg.values();
    return true;
}

} // namespace ui::dialogs
