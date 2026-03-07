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

#pragma once

#include "agent/provider.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace ui::widgets {

class QAgentPanelWidget : public QWidget {
    Q_OBJECT

  public:
    explicit QAgentPanelWidget(QWidget *parent = nullptr);

    void setProvider(agent::Provider *provider);
    void setScreenContext(const QString &screenText);
    void appendUserMessage(const QString &text);
    void appendAssistantMessage(const QString &text);
    void appendError(const QString &text);

  signals:
    void messageSubmitted(const QString &text);

  private:
    void setInputEnabled(bool enabled);

    QLabel *m_headerLabel;
    QTextEdit *m_chatHistory;
    QLineEdit *m_inputField;
    QPushButton *m_sendButton;

    agent::Provider *m_provider = nullptr;
    QString m_screenContext;
};

} // namespace ui::widgets
