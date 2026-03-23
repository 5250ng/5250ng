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

#include "session_settings_dialog.h"
#include "ui/themes/manager.h"
#include "ui/themes/terminal_theme.h"
#include "ui/widgets/Frameless/BaseFramelessDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace agent { class OAuthAuth; }

class SettingsDialog : public ui::widgets::BaseFramelessDialog {
    Q_OBJECT

  public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    /// Access the embedded 5250 theme editor so callers can wire up signals.
    SessionSettingsDialog *terminalThemeEditor() const { return m_terminalThemePage; }

    /// Pre-select a terminal theme in the embedded editor.
    void setTerminalTheme(const ui::themes::TerminalTheme &theme);

  signals:
    void terminalThemeApplyRequested(const ui::themes::TerminalTheme &theme);
    void terminalThemeApplyToAllRequested(const ui::themes::TerminalTheme &theme);
    void agentConfigChanged();

  private slots:
    void onCategoryChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onThemeChanged();
    void onApplyThemeClicked();
    void onCloseClicked();

  private:
    void setupUI();
    QWidget *buildThemePage();
    QWidget *buildMacrosPage();
    QWidget *buildAgentPage();
    void ensureThemesLoaded();
    void onAgentProviderChanged(int index);
    void onAgentAuthTypeChanged(int index);
    void onAgentTestClicked();
    void onAgentSaveClicked();
    void onOAuthSignInClicked();
    void onOAuthSignOutClicked();
    void updateOAuthStatus();
    void onMacrosSaveClicked();

    QSplitter *m_splitter;
    QTreeWidget *m_categoryTree;
    QStackedWidget *m_pages;

    // Application Theme page widgets
    QWidget *m_themePage;
    QComboBox *m_themeCombo;
    QPushButton *m_applyThemeBtn;

    // 5250 Theme page (embedded SessionSettingsDialog)
    SessionSettingsDialog *m_terminalThemePage;

    // Macros page widgets
    QWidget *m_macrosPage;
    QCheckBox *m_recordTimingsCheck;
    QPushButton *m_macrosSaveBtn;

    // Agent page widgets
    QWidget *m_agentPage;
    QComboBox *m_agentProviderCombo;
    QComboBox *m_agentAuthTypeCombo;
    QStackedWidget *m_authStack;

    // API Key auth widgets (stack index 0)
    QLineEdit *m_agentApiKeyEdit;
    QPushButton *m_agentTestBtn;

    // OAuth auth widgets (stack index 1)
    QLineEdit *m_oauthClientIdEdit;
    QLineEdit *m_oauthAuthEndpointEdit;
    QLineEdit *m_oauthTokenEndpointEdit;
    QLineEdit *m_oauthScopeEdit;
    QPushButton *m_oauthSignInBtn;
    QPushButton *m_oauthSignOutBtn;
    QLabel *m_oauthStatusLabel;

    QComboBox *m_agentModelCombo;
    QTextEdit *m_agentSystemPromptEdit;
    QPushButton *m_agentSaveBtn;

    agent::OAuthAuth *m_activeOAuth = nullptr;
};
