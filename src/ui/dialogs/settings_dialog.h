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
#include <QMap>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
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
    void onCloseClicked();
    void onSaveClicked();

  private:
    void setupUI();
    QWidget *buildThemePage();
    QWidget *buildScriptsGeneralPage();
    QWidget *buildMacrosPage();
    QWidget *buildAgentPage();
    QWidget *buildMcpServerPage();
    QWidget *buildToolsPage();
    void onToolsTableSelectionChanged();
    void ensureThemesLoaded();
    void onAgentProviderChanged(int index);
    void onAgentAuthTypeChanged(int index);
    void onAgentTestClicked();
    void onAgentSaveClicked();
    void onOAuthSignInClicked();
    void onOAuthSignOutClicked();
    void updateOAuthStatus();
    void onMacrosSaveClicked();
    void onMcpSaveClicked();

    QSplitter *m_splitter;
    QTreeWidget *m_categoryTree;
    QStackedWidget *m_pages;

    // Application Theme page widgets
    QWidget *m_themePage;
    QComboBox *m_themeCombo;

    // 5250 Theme page (embedded SessionSettingsDialog)
    SessionSettingsDialog *m_terminalThemePage;

    // Bottom bar
    QPushButton *m_saveBtn;

    // Scripts General page widgets
    QWidget *m_scriptsGeneralPage;
    QLineEdit *m_scriptsDirEdit;
    QSpinBox *m_defaultTimeoutSpin;
    QSpinBox *m_defaultDelaySpin;
    QSpinBox *m_defaultJitterMinSpin;
    QSpinBox *m_defaultJitterMaxSpin;
    QCheckBox *m_confirmDeleteCheck;

    // Scripts Recording page widgets (formerly Macros)
    QWidget *m_macrosPage;
    QCheckBox *m_recordTimingsCheck;

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
    QCheckBox *m_autoAcceptAllCheck;
    QCheckBox *m_autoAcceptToolCallsCheck;
    QCheckBox *m_autoAcceptFileEditsCheck;

    // MCP Server page widgets
    QWidget *m_mcpPage;
    QCheckBox *m_mcpAutoStartCheck;
    QSpinBox *m_mcpPortSpin;

    // Tools page (unified)
    QWidget *m_toolsPage;
    QTableWidget *m_toolsTable;
    QLabel *m_toolDetailDefaultDesc;
    QTextEdit *m_toolDetailCustomDesc;
    QLabel *m_toolDetailParams;
    QStringList m_toolNames;

    agent::OAuthAuth *m_activeOAuth = nullptr;
};
