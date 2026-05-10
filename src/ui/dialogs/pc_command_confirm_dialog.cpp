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

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace ui::dialogs {

namespace {

// Project-wide horizontal-rectangle dialog dimensions for STRPCCMD surfaces.
// Picked so the displayed command fits a typical PCCMD payload (123 char max
// per SA21-9247-6 §15.7) on one line at the project's default font.
constexpr int kDialogWidth = 720;
constexpr int kDialogHeight = 260;
constexpr int kIconSize = 48;

// Build the icon column shared by both surfaces (Allow/Deny and Deny/alert).
// QStyle::SP_MessageBoxWarning matches the system "this needs your attention"
// icon and keeps the dialog readable across platform themes.
QLabel *makeWarningIcon(QWidget *parent) {
    QLabel *icon = new QLabel(parent);
    QStyle *style = parent ? parent->style() : QApplication::style();
    QIcon stdIcon = style->standardIcon(QStyle::SP_MessageBoxWarning);
    icon->setPixmap(stdIcon.pixmap(kIconSize, kIconSize));
    icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    icon->setFixedWidth(kIconSize + 8);
    return icon;
}

QPlainTextEdit *makeCommandView(QWidget *parent, const QString &command) {
    QPlainTextEdit *view = new QPlainTextEdit(parent);
    view->setPlainText(command);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view->setMaximumHeight(70);
    return view;
}

} // namespace

PcCommandConfirmDialog::PcCommandConfirmDialog(const QString &hostname,
                                               const QString &command,
                                               QWidget *parent)
    : ui::widgets::BaseFramelessDialog(parent) {
    setWindowTitle("Allow PC command from host?");
    setModal(true);
    resize(kDialogWidth, kDialogHeight);

    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(16, 12, 16, 12);
    content->setSpacing(10);

    // Top row: icon column on the left, header + command + warning stacked
    // on the right. The icon column anchors the dialog as a horizontal
    // rectangle and matches the rest of the warning-style modals.
    QHBoxLayout *body = new QHBoxLayout();
    body->setSpacing(12);
    body->addWidget(makeWarningIcon(this));

    QVBoxLayout *text = new QVBoxLayout();
    text->setSpacing(8);

    QLabel *header = new QLabel(this);
    header->setTextFormat(Qt::RichText);
    header->setWordWrap(true);
    header->setText(
        QString("<b>The host <code>%1</code> is asking 5250ng to run a "
                "command on this PC.</b>")
            .arg(hostname.isEmpty() ? "(unknown)" : hostname.toHtmlEscaped()));
    text->addWidget(header);

    text->addWidget(makeCommandView(this, command));

    QLabel *warning = new QLabel(this);
    warning->setTextFormat(Qt::RichText);
    warning->setWordWrap(true);
    warning->setText(
        "<i>Allowing this will run the command above with your user account. "
        "It can read or modify any file you can, and contact any network you "
        "can reach. Only allow if you trust this host and recognise the "
        "command.</i>");
    text->addWidget(warning);

    body->addLayout(text, 1);
    content->addLayout(body, 1);

    // Buttons row: right-aligned, Deny is the default. Allow remains opt-in
    // — the user must explicitly select it.
    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *deny = new QPushButton("Deny", this);
    QPushButton *allow = new QPushButton("Allow", this);
    deny->setDefault(true);
    deny->setAutoDefault(true);
    allow->setDefault(false);
    allow->setAutoDefault(false);
    connect(deny, &QPushButton::clicked, this, &QDialog::reject);
    connect(allow, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(deny);
    buttons->addWidget(allow);
    content->addLayout(buttons);
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
    ui::widgets::BaseFramelessDialog dlg(parent);
    dlg.setWindowTitle("PC command blocked");
    dlg.setModal(true);
    dlg.resize(kDialogWidth, kDialogHeight);

    QVBoxLayout *content = dlg.contentLayout();
    content->setContentsMargins(16, 12, 16, 12);
    content->setSpacing(10);

    QHBoxLayout *body = new QHBoxLayout();
    body->setSpacing(12);
    body->addWidget(makeWarningIcon(&dlg));

    QVBoxLayout *text = new QVBoxLayout();
    text->setSpacing(8);

    QLabel *header = new QLabel(&dlg);
    header->setTextFormat(Qt::RichText);
    header->setWordWrap(true);
    header->setText(
        QString("<b>The host <code>%1</code> tried to run a command on this "
                "PC. It was refused by your session policy (Deny and alert).</b>")
            .arg(hostname.isEmpty() ? "(unknown)" : hostname.toHtmlEscaped()));
    text->addWidget(header);

    text->addWidget(makeCommandView(&dlg, command));

    QLabel *footer = new QLabel(&dlg);
    footer->setTextFormat(Qt::RichText);
    footer->setWordWrap(true);
    footer->setText(
        "<i>Nothing was executed. To allow PC commands from this host, change "
        "the STRPCCMD policy in the session settings.</i>");
    text->addWidget(footer);

    body->addLayout(text, 1);
    content->addLayout(body, 1);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *ok = new QPushButton("OK", &dlg);
    ok->setDefault(true);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    buttons->addWidget(ok);
    content->addLayout(buttons);

    dlg.exec();
}

} // namespace ui::dialogs
