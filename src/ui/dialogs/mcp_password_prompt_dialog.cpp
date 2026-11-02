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

#include "mcp_password_prompt_dialog.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace ui::dialogs {

namespace {

constexpr int kDialogWidth = 560;
constexpr int kDialogHeight = 220;
constexpr int kIconSize = 48;

} // namespace

McpPasswordPromptDialog::McpPasswordPromptDialog(const QString &hostname,
                                                 const QString &username,
                                                 QWidget *parent)
    : qt_ui_style::BaseFramelessDialog(parent) {
    setWindowTitle("MCP sign-on password");
    setModal(true);
    resize(kDialogWidth, kDialogHeight);

    QVBoxLayout *content = contentLayout();
    content->setContentsMargins(16, 12, 16, 12);
    content->setSpacing(10);

    QHBoxLayout *body = new QHBoxLayout();
    body->setSpacing(12);

    QLabel *icon = new QLabel(this);
    icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxQuestion)
                        .pixmap(kIconSize, kIconSize));
    icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    icon->setFixedWidth(kIconSize + 8);
    body->addWidget(icon);

    QVBoxLayout *text = new QVBoxLayout();
    text->setSpacing(8);

    QLabel *header = new QLabel(this);
    header->setTextFormat(Qt::RichText);
    header->setWordWrap(true);
    header->setText(
        QString("<b>An MCP client wants to sign on to <code>%1</code> as "
                "<code>%2</code>.</b>")
            .arg(hostname.isEmpty() ? "(unknown)" : hostname.toHtmlEscaped(),
                 username.toHtmlEscaped()));
    text->addWidget(header);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("Password");
    text->addWidget(m_passwordEdit);

    QLabel *note = new QLabel(this);
    note->setTextFormat(Qt::RichText);
    note->setWordWrap(true);
    note->setText(
        "<i>5250ng types the password into the sign-on screen itself. It is "
        "not sent to the MCP client. Cancel if you did not expect this "
        "request.</i>");
    text->addWidget(note);

    body->addLayout(text, 1);
    content->addLayout(body, 1);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *cancel = new QPushButton("Cancel", this);
    QPushButton *signOn = new QPushButton("Sign on", this);
    cancel->setAutoDefault(false);
    signOn->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(signOn, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(cancel);
    buttons->addWidget(signOn);
    content->addLayout(buttons);

    m_passwordEdit->setFocus();
}

QString McpPasswordPromptDialog::password() const {
    return m_passwordEdit->text();
}

bool McpPasswordPromptDialog::ask(const QString &hostname,
                                  const QString &username,
                                  QString *password,
                                  QWidget *parent,
                                  int timeoutMs) {
    McpPasswordPromptDialog dlg(hostname, username, parent);
    QTimer::singleShot(timeoutMs, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    if (password) *password = dlg.password();
    return true;
}

} // namespace ui::dialogs
