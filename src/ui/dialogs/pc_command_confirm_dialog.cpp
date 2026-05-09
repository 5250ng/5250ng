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

#include "pc_command_confirm_dialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace ui::dialogs {

PcCommandConfirmDialog::PcCommandConfirmDialog(const QString &hostname,
                                               const QString &command,
                                               QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Allow PC command from host?");
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *header = new QLabel(this);
    header->setTextFormat(Qt::RichText);
    header->setWordWrap(true);
    header->setText(
        QString("<b>The host <code>%1</code> is asking 5250ng to run a "
                "command on this PC.</b>")
            .arg(hostname.isEmpty() ? "(unknown)" : hostname.toHtmlEscaped()));
    layout->addWidget(header);

    auto *commandView = new QPlainTextEdit(this);
    commandView->setPlainText(command);
    commandView->setReadOnly(true);
    commandView->setMinimumHeight(80);
    layout->addWidget(commandView);

    auto *warning = new QLabel(this);
    warning->setTextFormat(Qt::RichText);
    warning->setWordWrap(true);
    warning->setText(
        "<i>Allowing this will run the command above with your user account. "
        "It can read or modify any file you can, and contact any network you "
        "can reach. Only allow if you trust this host and recognise the "
        "command.</i>");
    layout->addWidget(warning);

    auto *buttons = new QHBoxLayout();
    auto *deny = new QPushButton("Deny", this);
    auto *allow = new QPushButton("Allow", this);
    deny->setDefault(true);
    deny->setAutoDefault(true);
    allow->setDefault(false);
    allow->setAutoDefault(false);
    connect(deny, &QPushButton::clicked, this, &QDialog::reject);
    connect(allow, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addStretch();
    buttons->addWidget(deny);
    buttons->addWidget(allow);
    layout->addLayout(buttons);

    setLayout(layout);
}

bool PcCommandConfirmDialog::ask(const QString &hostname,
                                 const QString &command,
                                 QWidget *parent) {
    PcCommandConfirmDialog dlg(hostname, command, parent);
    return dlg.exec() == QDialog::Accepted;
}

void PcCommandConfirmDialog::notifyDenied(const QString &hostname,
                                          const QString &command,
                                          QWidget *parent) {
    // Information-only modal: surfaces a refused STRPCCMD attempt to the user
    // so a host that tries to run something never goes unnoticed under the
    // "Deny and alert" policy. No Allow button — the policy already refused.
    QDialog dlg(parent);
    dlg.setWindowTitle("PC command blocked");
    dlg.setModal(true);

    auto *layout = new QVBoxLayout(&dlg);

    auto *header = new QLabel(&dlg);
    header->setTextFormat(Qt::RichText);
    header->setWordWrap(true);
    header->setText(
        QString("<b>The host <code>%1</code> tried to run a command on this "
                "PC. It was refused by your session policy (Deny and alert).</b>")
            .arg(hostname.isEmpty() ? "(unknown)" : hostname.toHtmlEscaped()));
    layout->addWidget(header);

    auto *commandView = new QPlainTextEdit(&dlg);
    commandView->setPlainText(command);
    commandView->setReadOnly(true);
    commandView->setMinimumHeight(80);
    layout->addWidget(commandView);

    auto *footer = new QLabel(&dlg);
    footer->setTextFormat(Qt::RichText);
    footer->setWordWrap(true);
    footer->setText(
        "<i>Nothing was executed. To allow PC commands from this host, change "
        "the STRPCCMD policy in the session settings.</i>");
    layout->addWidget(footer);

    auto *buttons = new QHBoxLayout();
    auto *ok = new QPushButton("OK", &dlg);
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    buttons->addStretch();
    buttons->addWidget(ok);
    layout->addLayout(buttons);

    dlg.setLayout(layout);
    dlg.exec();
}

} // namespace ui::dialogs
