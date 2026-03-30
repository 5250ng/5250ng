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

#include "StyledColorDialog.h"
#include <QVBoxLayout>

namespace ui::widgets {

StyledColorDialog::StyledColorDialog(QWidget *parent, const QColor &initial,
                                     const QString &title, bool showAlpha)
    : BaseFramelessDialog(parent), m_selectedColor(initial) {
    setWindowTitle(title.isEmpty() ? "Select Color" : title);
    setModal(true);

    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);

    m_colorDialog = new QColorDialog(this);
    m_colorDialog->setWindowFlags(Qt::Widget);
    m_colorDialog->setOptions(QColorDialog::DontUseNativeDialog
                              | QColorDialog::NoButtons
                              | (showAlpha ? QColorDialog::ShowAlphaChannel
                                           : QColorDialog::ColorDialogOptions()));
    // Set initial color AFTER options — DontUseNativeDialog resets internal
    // state, so passing it via constructor may lose it on some platforms.
    m_colorDialog->setCurrentColor(initial);
    content->addWidget(m_colorDialog);

    // Add our own buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(16, 8, 16, 12);
    btnLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton *okBtn = new QPushButton("OK", this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        m_selectedColor = m_colorDialog->currentColor();
        accept();
    });
    btnLayout->addWidget(okBtn);

    content->addLayout(btnLayout);
}

QColor StyledColorDialog::getColor(const QColor &initial, QWidget *parent,
                                   const QString &title, bool showAlpha) {
    StyledColorDialog dlg(parent, initial, title, showAlpha);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_selectedColor;
    }
    return QColor(); // invalid color signals cancellation
}

} // namespace ui::widgets
