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

#include "StyledFileDialog.h"
#include <QVBoxLayout>

namespace ui::widgets {

StyledFileDialog::StyledFileDialog(QWidget *parent, const QString &caption,
                                   const QString &dir, const QString &filter,
                                   QFileDialog::AcceptMode acceptMode)
    : BaseFramelessDialog(parent) {
    setWindowTitle(caption.isEmpty() ? (acceptMode == QFileDialog::AcceptOpen ? "Open File" : "Save File") : caption);
    setModal(true);

    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);

    m_fileDialog = new QFileDialog(this, QString(), dir, filter);
    m_fileDialog->setWindowFlags(Qt::Widget);
    m_fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    m_fileDialog->setAcceptMode(acceptMode);
    if (acceptMode == QFileDialog::AcceptOpen) {
        m_fileDialog->setFileMode(QFileDialog::ExistingFile);
    } else {
        m_fileDialog->setFileMode(QFileDialog::AnyFile);
    }
    content->addWidget(m_fileDialog);

    connect(m_fileDialog, &QFileDialog::accepted, this, &QDialog::accept);
    connect(m_fileDialog, &QFileDialog::rejected, this, &QDialog::reject);

    resize(700, 500);
}

QString StyledFileDialog::getOpenFileName(QWidget *parent,
                                          const QString &caption,
                                          const QString &dir,
                                          const QString &filter) {
    StyledFileDialog dlg(parent, caption, dir, filter, QFileDialog::AcceptOpen);
    if (dlg.exec() == QDialog::Accepted) {
        QStringList files = dlg.m_fileDialog->selectedFiles();
        return files.isEmpty() ? QString() : files.first();
    }
    return {};
}

QString StyledFileDialog::getSaveFileName(QWidget *parent,
                                          const QString &caption,
                                          const QString &dir,
                                          const QString &filter) {
    StyledFileDialog dlg(parent, caption, dir, filter, QFileDialog::AcceptSave);
    if (dlg.exec() == QDialog::Accepted) {
        QStringList files = dlg.m_fileDialog->selectedFiles();
        return files.isEmpty() ? QString() : files.first();
    }
    return {};
}

} // namespace ui::widgets
