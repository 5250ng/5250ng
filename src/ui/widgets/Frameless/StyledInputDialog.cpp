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

#include "StyledInputDialog.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui::widgets {

StyledInputDialog::StyledInputDialog(QWidget *parent, const QString &title,
                                     const QString &label,
                                     QLineEdit::EchoMode mode,
                                     const QString &text)
    : BaseFramelessDialog(parent) {
    setWindowTitle(title);
    setModal(true);

    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(16, 12, 16, 12);
    content->setSpacing(12);

    QLabel *lbl = new QLabel(label, this);
    lbl->setWordWrap(true);
    content->addWidget(lbl);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setEchoMode(mode);
    m_lineEdit->setText(text);
    m_lineEdit->selectAll();
    content->addWidget(m_lineEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton *okBtn = new QPushButton("OK", this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(okBtn);

    content->addLayout(btnLayout);

    m_lineEdit->setFocus();
}

QString StyledInputDialog::getText(QWidget *parent, const QString &title,
                                   const QString &label,
                                   QLineEdit::EchoMode mode,
                                   const QString &text, bool *ok) {
    StyledInputDialog dlg(parent, title, label, mode, text);
    bool accepted = (dlg.exec() == QDialog::Accepted);
    if (ok) *ok = accepted;
    return accepted ? dlg.m_lineEdit->text() : QString();
}

} // namespace ui::widgets
