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

#include "StyledMessageBox.h"
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace ui::widgets {

StyledMessageBox::StyledMessageBox(QWidget *parent, const QString &title,
                                   const QString &text, bool hasNo)
    : BaseFramelessDialog(parent) {
    setWindowTitle(title);
    setModal(true);

    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(16, 12, 16, 12);
    content->setSpacing(12);

    QLabel *label = new QLabel(text, this);
    label->setWordWrap(true);
    label->setMinimumWidth(280);
    content->addWidget(label);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    if (hasNo) {
        QPushButton *noBtn = new QPushButton("No", this);
        connect(noBtn, &QPushButton::clicked, this, [this]() {
            m_result = No;
            reject();
        });
        btnLayout->addWidget(noBtn);

        QPushButton *yesBtn = new QPushButton("Yes", this);
        yesBtn->setDefault(true);
        connect(yesBtn, &QPushButton::clicked, this, [this]() {
            m_result = Yes;
            accept();
        });
        btnLayout->addWidget(yesBtn);
    } else {
        QPushButton *okBtn = new QPushButton("OK", this);
        okBtn->setDefault(true);
        connect(okBtn, &QPushButton::clicked, this, [this]() {
            m_result = Ok;
            accept();
        });
        btnLayout->addWidget(okBtn);
    }

    content->addLayout(btnLayout);
}

void StyledMessageBox::information(QWidget *parent, const QString &title, const QString &text) {
    StyledMessageBox dlg(parent, title, text);
    dlg.exec();
}

void StyledMessageBox::warning(QWidget *parent, const QString &title, const QString &text) {
    StyledMessageBox dlg(parent, title, text);
    dlg.exec();
}

StyledMessageBox::Result StyledMessageBox::question(QWidget *parent, const QString &title, const QString &text) {
    StyledMessageBox dlg(parent, title, text, true);
    dlg.exec();
    return dlg.m_result;
}

} // namespace ui::widgets
