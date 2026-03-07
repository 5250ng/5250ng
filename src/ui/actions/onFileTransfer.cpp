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

#include "../main_window.h"
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>

void MainWindow::onFileTransfer() {
    if (m_activeIndex < 0 || m_activeIndex >= m_sessions.size()) return;

    Session *session = m_sessions[m_activeIndex];
    // Capture the session index so lambdas can re-validate instead of using
    // a raw pointer that may dangle if the tab is closed while the dialog is open.
    int sessionIndex = m_activeIndex;

    // If credentials are missing, prompt the user
    if (session->config.username().isEmpty() || session->config.password().isEmpty()) {
        auto *credDialog = new ui::widgets::BaseFramelessDialog(this);
        credDialog->setWindowTitle(QStringLiteral("File Transfer - Credentials Required"));
        credDialog->setAttribute(Qt::WA_DeleteOnClose);
        credDialog->setModal(true);
        credDialog->resize(350, 180);

        auto *layout = credDialog->contentLayout();
        layout->setContentsMargins(12, 8, 12, 12);
        layout->setSpacing(8);

        auto *infoLabel = new QLabel(
            QStringLiteral("Enter credentials for %1:").arg(session->config.hostname()));
        layout->addWidget(infoLabel);

        auto *formLayout = new QFormLayout;
        auto *usernameEdit = new QLineEdit;
        usernameEdit->setPlaceholderText(QStringLiteral("User ID"));
        if (!session->config.username().isEmpty())
            usernameEdit->setText(session->config.username());

        auto *passwordEdit = new QLineEdit;
        passwordEdit->setPlaceholderText(QStringLiteral("Password"));
        passwordEdit->setEchoMode(QLineEdit::Password);

        formLayout->addRow(QStringLiteral("Username:"), usernameEdit);
        formLayout->addRow(QStringLiteral("Password:"), passwordEdit);
        layout->addLayout(formLayout);

        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        auto *okButton = new QPushButton(QStringLiteral("Connect"));
        auto *cancelButton = new QPushButton(QStringLiteral("Cancel"));
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        layout->addLayout(buttonLayout);

        connect(cancelButton, &QPushButton::clicked, credDialog, &QDialog::reject);
        connect(okButton, &QPushButton::clicked, credDialog, [this, sessionIndex, usernameEdit, passwordEdit, credDialog]() {
            QString user = usernameEdit->text().trimmed();
            QString pass = passwordEdit->text();
            if (user.isEmpty() || pass.isEmpty()) return;
            // Re-validate session is still alive
            if (sessionIndex < 0 || sessionIndex >= m_sessions.size()) return;

            m_sessions[sessionIndex]->config.setUsername(user);
            m_sessions[sessionIndex]->config.setPassword(pass);
            credDialog->accept();
        });

        connect(credDialog, &QDialog::accepted, this, [this, sessionIndex]() {
            // Re-validate session is still alive after async dialog
            if (sessionIndex < 0 || sessionIndex >= m_sessions.size()) return;
            auto *dialog = new FileTransferDialog(m_sessions[sessionIndex]->config, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        });

        // Focus the first empty field
        if (usernameEdit->text().isEmpty()) {
            usernameEdit->setFocus();
        } else {
            passwordEdit->setFocus();
        }

        credDialog->show();
        return;
    }

    auto *dialog = new FileTransferDialog(session->config, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
